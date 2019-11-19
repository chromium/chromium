// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_manager.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/ios/block_types.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/payments/core/can_make_payment_query.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_app.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_validation.h"
#include "components/payments/core/payment_prefs.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_response.h"
#include "components/payments/core/payment_shipping_option.h"
#include "components/payments/core/web_payment_request.h"
#include "components/payments/mojom/payment_request_data.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/autofill/validation_rules_storage_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/payments/ios_can_make_payment_query_factory.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher.h"
#include "ios/chrome/browser/payments/ios_payment_instrument_launcher_factory.h"
#include "ios/chrome/browser/payments/ios_payment_request_cache_factory.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_cache.h"
#import "ios/chrome/browser/payments/payment_response_helper.h"
#include "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/payments/js_payment_request_manager.h"
#import "ios/chrome/browser/ui/payments/payment_request_coordinator.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_coordinator.h"
#include "ios/web/common/origin_util.h"
#import "ios/web/common/url_scheme_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#include "ios/web/public/favicon/favicon_status.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kAbortError = @"AbortError";
NSString* const kInvalidStateError = @"InvalidStateError";
NSString* const kNotAllowedError = @"NotAllowedError";
NSString* const kNotSupportedError = @"NotSupportedError";

namespace {

// Command prefix for injected JavaScript.
const char kCommandPrefix[] = "paymentRequest";

// Time interval between attempts to unblock the webview's JS event queue.
const NSTimeInterval kNoopInterval = 0.1;

// Time interval before closing the UI if the page has not yet called
// PaymentResponse.complete().
const NSTimeInterval kTimeoutInterval = 60.0;

// Error messages used in Payment Request API.
NSString* const kCancelErrorMessage = @"User closed the Payment Request UI.";

struct PendingPaymentResponse {
  std::string methodName;
  std::string stringifiedDetails;
  autofill::AutofillProfile shippingAddress;
  autofill::AutofillProfile contactAddress;
};

}  // namespace

@interface PaymentRequestManager ()<CRWWebStateObserver,
                                    PaymentRequestCoordinatorDelegate,
                                    PaymentRequestErrorCoordinatorDelegate,
                                    PaymentRequestUIDelegate,
                                    PaymentResponseHelperConsumer> {
  // View controller used to present the PaymentRequest view controller.
  __weak UIViewController* _baseViewController;

  // PersonalDataManager used to manage user credit cards and addresses.
  autofill::PersonalDataManager* _personalDataManager;

  // The observer for |_activeWebState|.
  std::unique_ptr<web::WebStateObserverBridge> _activeWebStateObserver;

  // Timer used to periodically unblock the webview's JS event queue.
  NSTimer* _unblockEventQueueTimer;

  // Timer used to complete the Payment Request flow and close the UI if the
  // page does not call PaymentResponse.complete() in a timely fashion.
  NSTimer* _paymentResponseTimeoutTimer;

  // Timer used to cancel the Payment Request flow and close the UI if the
  // page does not settle the pending update promise in a timely fashion.
  NSTimer* _updateEventTimeoutTimer;

  // Storage for data to return in the payment response, until we're ready to
  // send an actual PaymentResponse.
  PendingPaymentResponse _pendingPaymentResponse;

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> _subscription;
}

// YES if Payment Request is enabled on the active web state.
@property(readonly) BOOL enabled;

// The ios::ChromeBrowserState instance passed to the initializer.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// Coordinator used to create and present the PaymentRequest view controller.
@property(nonatomic, strong)
    PaymentRequestCoordinator* paymentRequestCoordinator;

// Coordinator used to create and present the PaymentRequest error view
// controller.
@property(nonatomic, strong)
    PaymentRequestErrorCoordinator* paymentRequestErrorCoordinator;

// Object that manages JavaScript injection into the web view.
@property(nonatomic, weak) JSPaymentRequestManager* paymentRequestJsManager;

// Maintains a map of web::WebState to a list of payments::PaymentRequest
// instances maintained for that WebState.
@property(nonatomic, assign) payments::PaymentRequestCache* paymentRequestCache;

// The payments::PaymentRequest instance currently showing, if any.
@property(nonatomic, assign) payments::PaymentRequest* pendingPaymentRequest;

// The dispatcher for Payment Requests.
@property(nonatomic, weak, readonly) id<ApplicationCommands> dispatcher;

// Dismisses the UI, rejects the pending request promise with "AbortError" and
// |errorMessage|, and records |reason| for the pending request. Invokes
// |callback| once request promise is rejected.
- (void)dismissUIAndAbortPendingRequestWithReason:
            (payments::JourneyLogger::AbortReason)reason
                                     errorMessage:(NSString*)errorMessage
                                         callback:
                                             (ProceduralBlockWithBool)callback;

// Rejects the pending request promise with "AbortError" and |errorMessage|, and
// records |reason| for the pending request. Invokes |callback| once request
// promise is rejected.
- (void)abortPendingRequestWithReason:
            (payments::JourneyLogger::AbortReason)reason
                         errorMessage:(NSString*)errorMessage
                             callback:(ProceduralBlockWithBool)callback;

// Rejects the pending request promise with |errorName| and |errorMessage|, and
// records |reason| for |paymentRequest|. Invokes |callback| once request
// promise is rejected. |paymentRequest| may be nil.
- (void)abortPaymentRequest:(payments::PaymentRequest*)paymentRequest
                     reason:(payments::JourneyLogger::AbortReason)reason
                  errorName:(NSString*)errorName
               errorMessage:(NSString*)errorMessage
                   callback:(ProceduralBlockWithBool)callback;

// Handler for injected JavaScript callbacks.
- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand;

// Handles creation of a PaymentRequest instance. The value of the JavaScript
// PaymentRequest object should be provided in |message|. Returns YES if the
// invocation was successful.
- (BOOL)handleCreatePaymentRequest:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.show(). The value of the JavaScript
// PaymentRequest object should be provided in |message|. Returns YES if the
// invocation was successful.
- (BOOL)handleRequestShow:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.abort(). Returns YES if the invocation
// was successful.
- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequest.canMakePayment(). Returns YES if the
// invocation was successful.
- (BOOL)handleCanMakePayment:(const base::DictionaryValue&)message;

// Called by |_updateEventTimeoutTimer|, displays an error message. Upon
// dismissal of the error message, cancels the Payment Request as if it was
// performed by the user.
- (BOOL)displayErrorThenCancelRequest;

// Called by |_paymentResponseTimeoutTimer|, invokes handleResponseComplete:
// as if PaymentResponse.complete() was invoked with the default "unknown"
// argument.
- (BOOL)doResponseComplete;

// Handles invocations of PaymentResponse.complete(). Returns YES if the
// invocation was successful.
- (BOOL)handleResponseComplete:(const base::DictionaryValue&)message;

// Handles setting the "updating" state of the pending request. Returns YES if
// the invocation was successful.
- (BOOL)handleSetPendingRequestUpdating:(const base::DictionaryValue&)message;

// Handles invocations of PaymentRequestUpdateEvent.updateWith(). Returns YES if
// the invocation was successful.
- (BOOL)handleUpdatePaymentDetails:(const base::DictionaryValue&)message;

// Establishes a timer that periodically prompts the JS manager to execute a
// noop. This works around an issue where the JS event queue is blocked while
// presenting the Payment Request UI.
- (void)setUnblockEventQueueTimer;

// Establishes a timer that calls doResponseComplete when it times out. Per
// the spec, if the page does not call PaymentResponse.complete() within some
// timeout period, user agents may behave as if the complete() method was
// called with no arguments.
- (void)setPaymentResponseTimeoutTimer;

// Establishes a timer that dismisses the Payment Request UI when it times out.
// Per the spec, implementations may choose to consider a timeout for the
// promise provided with the PaymentRequestUpdateEvent.updateWith() call. If the
// promise doesn't get settled in a reasonable amount of time, it is as if it
// was rejected.
- (void)setUpdateEventTimeoutTimer;

// Returns the instance of payments::PaymentRequest for self.activeWebState that
// has the identifier |paymentRequestId|, if any. Otherwise returns nullptr.
- (payments::PaymentRequest*)paymentRequestWithId:(std::string)paymentRequestId;

// Invalidates timers and stops |paymentRequestCoordinator|. Invokes |callback|
// when the view controller is dismissed.
- (void)dismissPaymentRequestUIWithCallback:(ProceduralBlock)callback;

// Starts the error coordinator. Invokes |callback| when the error is dismissed.
- (void)displayErrorWithCallback:(ProceduralBlock)callback;

@end

@implementation PaymentRequestManager

@synthesize locationBarModel = _locationBarModel;
@synthesize browserState = _browserState;
@synthesize enabled = _enabled;
@synthesize activeWebState = _activeWebState;
@synthesize paymentRequestCoordinator = _paymentRequestCoordinator;
@synthesize paymentRequestErrorCoordinator = _paymentRequestErrorCoordinator;
@synthesize paymentRequestJsManager = _paymentRequestJsManager;
@synthesize paymentRequestCache = _paymentRequestCache;
@synthesize pendingPaymentRequest = _pendingPaymentRequest;
@synthesize dispatcher = _dispatcher;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                                dispatcher:(id<ApplicationCommands>)dispatcher {
  if ((self = [super init])) {
    _baseViewController = viewController;

    _browserState = browserState;

    _dispatcher = dispatcher;

    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());

    _paymentRequestCache =
        payments::IOSPaymentRequestCacheFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());

    _activeWebStateObserver =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  if (_activeWebState) {
    _paymentRequestJsManager = nil;

    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _activeWebStateObserver.reset();
    _activeWebState = nullptr;
  }
}

- (void)setActiveWebState:(web::WebState*)webState {
  if (_activeWebState) {
    _paymentRequestJsManager = nil;

    _activeWebState->RemoveObserver(_activeWebStateObserver.get());
    _subscription.reset();
    _activeWebState = nullptr;
  }

  _activeWebState = webState;

  if (_activeWebState) {
    __weak PaymentRequestManager* weakSelf = self;
    auto callback = base::BindRepeating(
        ^(const base::DictionaryValue& JSON, const GURL& originURL,
          bool userIsInteracting, web::WebFrame* senderFrame) {
          // Payment request is only supported on main frame.
          if (senderFrame->IsMainFrame()) {
            // |originURL| and |userIsInteracting| aren't used.
            [weakSelf handleScriptCommand:JSON];
          }
        });
    _activeWebState->AddObserver(_activeWebStateObserver.get());
    _subscription =
        _activeWebState->AddScriptCommandCallback(callback, kCommandPrefix);

    _paymentRequestJsManager =
        base::mac::ObjCCastStrict<JSPaymentRequestManager>(
            [_activeWebState->GetJSInjectionReceiver()
                instanceOfClass:[JSPaymentRequestManager class]]);
  }
}

- (void)stopTrackingWebState:(web::WebState*)webState {
  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlockWithBool callback = ^(BOOL) {
    for (const auto& paymentRequest :
         weakSelf.paymentRequestCache->GetPaymentRequests(webState)) {
      if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
        paymentRequest->journey_logger().SetAborted(
            payments::JourneyLogger::ABORT_REASON_USER_NAVIGATION);
        paymentRequest->set_updating(false);
        paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
      }
    }
    // The lifetime of a PaymentRequest is tied to the WebState it is associated
    // with and the current URL. Therefore, PaymentRequest instances should get
    // destroyed when the WebState goes away.
    weakSelf.paymentRequestCache->ClearPaymentRequests(webState);
  };

  // Abort any pending request.
  if (_pendingPaymentRequest) {
    [self dismissUIAndAbortPendingRequestWithReason:
              payments::JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION
                                       errorMessage:kCancelErrorMessage
                                           callback:callback];
  } else {
    callback(YES);
  }
}

- (void)enablePaymentRequest:(BOOL)enabled {
  if (_enabled == enabled)
    return;

  _enabled = enabled;
  if (!enabled)
    [self cancelRequest];
}

- (void)cancelRequest {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE) {
    return;
  }

  [self dismissUIAndAbortPendingRequestWithReason:
            payments::JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION
                                     errorMessage:kCancelErrorMessage
                                         callback:nil];
}

- (void)dismissUIAndAbortPendingRequestWithReason:
            (payments::JourneyLogger::AbortReason)reason
                                     errorMessage:(NSString*)errorMessage
                                         callback:
                                             (ProceduralBlockWithBool)callback {
  __weak PaymentRequestManager* weakSelf = self;
  [self dismissPaymentRequestUIWithCallback:^{
    [weakSelf abortPendingRequestWithReason:reason
                               errorMessage:errorMessage
                                   callback:callback];
  }];
}

- (void)abortPendingRequestWithReason:
            (payments::JourneyLogger::AbortReason)reason
                         errorMessage:(NSString*)errorMessage
                             callback:(ProceduralBlockWithBool)callback {
  DCHECK(_pendingPaymentRequest);
  DCHECK(_pendingPaymentRequest->state() ==
         payments::PaymentRequest::State::INTERACTIVE);

  [self abortPaymentRequest:self.pendingPaymentRequest
                     reason:reason
                  errorName:kAbortError
               errorMessage:errorMessage
                   callback:callback];
  self.pendingPaymentRequest = nullptr;
}

- (void)abortPaymentRequest:(payments::PaymentRequest*)paymentRequest
                     reason:(payments::JourneyLogger::AbortReason)reason
                  errorName:(NSString*)errorName
               errorMessage:(NSString*)errorMessage
                   callback:(ProceduralBlockWithBool)callback {
  if (paymentRequest &&
      paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
    paymentRequest->journey_logger().SetAborted(reason);
    paymentRequest->set_updating(false);
    paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
  }

  [_paymentRequestJsManager rejectRequestPromiseWithErrorName:errorName
                                                 errorMessage:errorMessage
                                            completionHandler:callback];
}

- (void)resetIOSPaymentInstrumentLauncherDelegate {
  payments::IOSPaymentInstrumentLauncher* paymentAppLauncher =
      payments::IOSPaymentInstrumentLauncherFactory::GetForBrowserState(
          _browserState->GetOriginalChromeBrowserState());
  DCHECK(paymentAppLauncher);
  paymentAppLauncher->set_delegate(nullptr);
}

- (void)close {
  [self setActiveWebState:nullptr];
}

- (BOOL)handleScriptCommand:(const base::DictionaryValue&)JSONCommand {
  // Early return if the Payment Request is not enabled.
  if (!_enabled)
    return NO;

  std::string command;
  if (!JSONCommand.GetString("command", &command)) {
    LOG(ERROR) << "Received bad JSON: No 'command' field";
    return NO;
  }

  if (command == "paymentRequest.createPaymentRequest") {
    return [self handleCreatePaymentRequest:JSONCommand];
  }
  if (command == "paymentRequest.requestShow") {
    return [self handleRequestShow:JSONCommand];
  }
  if (command == "paymentRequest.requestAbort") {
    return [self handleRequestAbort:JSONCommand];
  }
  if (command == "paymentRequest.requestCanMakePayment") {
    return [self handleCanMakePayment:JSONCommand];
  }
  if (command == "paymentRequest.responseComplete") {
    return [self handleResponseComplete:JSONCommand];
  }
  if (command == "paymentRequest.setPendingRequestUpdating") {
    return [self handleSetPendingRequestUpdating:JSONCommand];
  }
  if (command == "paymentRequest.updatePaymentDetails") {
    return [self handleUpdatePaymentDetails:JSONCommand];
  }
  return NO;
}

// Extracts a payments::WebPaymentRequest from |message|. Creates and returns an
// instance of payments::PaymentRequest which is initialized with the
// payments::WebPaymentRequest object. Returns nullptr and populates
// |errorMessage| with the appropriate error message if it cannot extract a
// payments::WebPaymentRequest from |message| or the payments::WebPaymentRequest
// instance is invalid.
- (payments::PaymentRequest*)
newPaymentRequestFromMessage:(const base::DictionaryValue&)message
                errorMessage:(std::string*)errorMessage {
  DCHECK(errorMessage);
  const base::DictionaryValue* paymentRequestData;
  payments::WebPaymentRequest webPaymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    *errorMessage = "JS message parameter 'payment_request' is missing";
    return nullptr;
  }
  if (!webPaymentRequest.FromDictionaryValue(*paymentRequestData)) {
    *errorMessage = "Cannot create payment request";
    return nullptr;
  }
  if (!payments::ValidatePaymentDetails(webPaymentRequest.details,
                                        errorMessage)) {
    return nullptr;
  }

  return _paymentRequestCache->AddPaymentRequest(
      _activeWebState, std::make_unique<payments::PaymentRequest>(
                           webPaymentRequest, _browserState, _activeWebState,
                           _personalDataManager, self));
}

// Extracts a payments::WebPaymentRequest from |message|. Returns the cached
// instance of payments::PaymentRequest that corresponds to the extracted
// payments::WebPaymentRequest object, if one exists. Otherwise, creates and
// returns a new one which is initialized with the payments::WebPaymentRequest
// object. Returns nullptr and populates |errorMessage| with the appropriate
// error message if it cannot extract a payments::WebPaymentRequest from
// |message|, cannot find the payments::PaymentRequest instance or the
// payments::WebPaymentRequest instance is invalid.
- (payments::PaymentRequest*)
paymentRequestFromMessage:(const base::DictionaryValue&)message
             errorMessage:(std::string*)errorMessage {
  DCHECK(errorMessage);
  const base::DictionaryValue* paymentRequestData;
  payments::WebPaymentRequest webPaymentRequest;
  if (!message.GetDictionary("payment_request", &paymentRequestData)) {
    *errorMessage = "JS message parameter 'payment_request' is missing";
    return nullptr;
  }
  if (!webPaymentRequest.FromDictionaryValue(*paymentRequestData)) {
    *errorMessage = "Cannot create payment request";
    return nullptr;
  }
  if (!payments::ValidatePaymentDetails(webPaymentRequest.details,
                                        errorMessage)) {
    return nullptr;
  }

  return [self paymentRequestWithId:webPaymentRequest.payment_request_id];
}

- (BOOL)handleCreatePaymentRequest:(const base::DictionaryValue&)message {
  std::string errorMessage;
  payments::PaymentRequest* paymentRequest =
      [self newPaymentRequestFromMessage:message errorMessage:&errorMessage];
  if (!paymentRequest) {
    LOG(ERROR) << errorMessage;
    [_paymentRequestJsManager
        throwDOMExceptionWithErrorName:kInvalidStateError
                          errorMessage:base::SysUTF8ToNSString(errorMessage)
                     completionHandler:nil];
  }
  return YES;
}

- (BOOL)handleRequestShow:(const base::DictionaryValue&)message {
  bool waitForShowPromise;
  if (!message.GetBoolean("waitForShowPromise", &waitForShowPromise)) {
    LOG(ERROR) << "JS message parameter 'waitForShowPromise' is missing";
    return NO;
  }

  std::string errorMessage;
  payments::PaymentRequest* paymentRequest =
      [self paymentRequestFromMessage:message errorMessage:&errorMessage];
  if (!paymentRequest) {
    LOG(ERROR) << "Request promise rejected: "
               << base::SysNSStringToUTF16(kInvalidStateError) << errorMessage;
    [self abortPaymentRequest:nil
                       reason:payments::JourneyLogger::ABORT_REASON_OTHER
                    errorName:kInvalidStateError
                 errorMessage:base::SysUTF8ToNSString(errorMessage)
                     callback:nil];
    return YES;
  }

  if (![self webStateContentIsSecureHTML]) {
    if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
      paymentRequest->journey_logger().SetNotShown(
          payments::JourneyLogger::NOT_SHOWN_REASON_OTHER);
      paymentRequest->set_updating(false);
      paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
    }
    LOG(ERROR) << "Request promise rejected: "
               << base::SysNSStringToUTF16(kNotSupportedError)
               << "Must be in a secure context";
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorName:kNotSupportedError
                             errorMessage:@"Must be in a secure context"
                        completionHandler:nil];
    return YES;
  }

  if (paymentRequest->state() != payments::PaymentRequest::State::CREATED) {
    if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
      paymentRequest->journey_logger().SetNotShown(
          payments::JourneyLogger::NOT_SHOWN_REASON_OTHER);
      paymentRequest->set_updating(false);
      paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
    }
    LOG(ERROR) << "Request promise rejected: "
               << base::SysNSStringToUTF16(kInvalidStateError)
               << "Already called show() once";
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorName:kInvalidStateError
                             errorMessage:@"Already called show() once"
                        completionHandler:nil];
    return YES;
  }

  if (_pendingPaymentRequest) {
    if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
      paymentRequest->journey_logger().SetNotShown(
          payments::JourneyLogger::NOT_SHOWN_REASON_CONCURRENT_REQUESTS);
      paymentRequest->set_updating(false);
      paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
    }
    LOG(ERROR) << "Request promise rejected: "
               << base::SysNSStringToUTF16(kAbortError)
               << "Only one PaymentRequest may be shown at a time";
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorName:kAbortError
                             errorMessage:@"Only one PaymentRequest may be "
                                          @"shown at a time"
                        completionHandler:nil];
    return YES;
  }

  if (paymentRequest->supported_card_networks().empty() &&
      (!base::FeatureList::IsEnabled(
           payments::features::kWebPaymentsNativeApps) ||
       paymentRequest->url_payment_method_identifiers().empty())) {
    if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
      paymentRequest->journey_logger().SetNotShown(
          payments::JourneyLogger::
              NOT_SHOWN_REASON_NO_SUPPORTED_PAYMENT_METHOD);
      paymentRequest->set_updating(false);
      paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
    }
    LOG(ERROR) << "Request promise rejected: "
               << base::SysNSStringToUTF16(kNotSupportedError)
               << "The payment method is not supported";
    [_paymentRequestJsManager
        rejectRequestPromiseWithErrorName:kNotSupportedError
                             errorMessage:@"The payment method is not supported"
                        completionHandler:nil];

    return YES;
  }

  _pendingPaymentRequest = paymentRequest;
  paymentRequest->set_state(payments::PaymentRequest::State::INTERACTIVE);

  paymentRequest->journey_logger().SetEventOccurred(
      payments::JourneyLogger::EVENT_SHOWN);

  UIImage* pageFavicon = nil;
  web::NavigationItem* navigationItem =
      _activeWebState->GetNavigationManager()->GetVisibleItem();
  if (navigationItem && !navigationItem->GetFavicon().image.IsEmpty())
    pageFavicon = navigationItem->GetFavicon().image.ToUIImage();
  NSString* pageTitle = base::SysUTF16ToNSString(_activeWebState->GetTitle());
  NSString* pageHost =
      base::SysUTF16ToNSString(url_formatter::FormatUrlForSecurityDisplay(
          _activeWebState->GetLastCommittedURL()));
  BOOL connectionSecure =
      _activeWebState->GetLastCommittedURL().SchemeIs(url::kHttpsScheme);
  // Payment Request is only enabled in main frame.
  web::WebFrame* main_frame =
      _activeWebState->GetWebFramesManager()->GetMainWebFrame();
  autofill::AutofillManager* autofillManager =
      autofill::AutofillDriverIOS::FromWebStateAndWebFrame(_activeWebState,
                                                           main_frame)
          ->autofill_manager();
  _paymentRequestCoordinator = [[PaymentRequestCoordinator alloc]
      initWithBaseViewController:_baseViewController];
  [_paymentRequestCoordinator setPaymentRequest:paymentRequest];
  [_paymentRequestCoordinator setAutofillManager:autofillManager];
  [_paymentRequestCoordinator setBrowserState:_browserState];
  [_paymentRequestCoordinator setPageFavicon:pageFavicon];
  [_paymentRequestCoordinator setPageTitle:pageTitle];
  [_paymentRequestCoordinator setPageHost:pageHost];
  [_paymentRequestCoordinator setConnectionSecure:connectionSecure];
  [_paymentRequestCoordinator setDelegate:self];

  [_paymentRequestCoordinator start];

  if (waitForShowPromise) {
    // Disable the UI and display the spinner.
    [_paymentRequestCoordinator setPending:waitForShowPromise];

    [self setUnblockEventQueueTimer];
    [self setUpdateEventTimeoutTimer];
  } else {
    paymentRequest->journey_logger().RecordTransactionAmount(
        paymentRequest->payment_details().total->amount->currency,
        paymentRequest->payment_details().total->amount->value,
        false /*completed*/);
  }

  return YES;
}

- (BOOL)handleRequestAbort:(const base::DictionaryValue&)message {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE) {
    return YES;
  }

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlockWithBool cancellationCallback = ^(BOOL) {
    [[weakSelf paymentRequestJsManager]
        resolveAbortPromiseWithCompletionHandler:nil];
  };

  ProceduralBlock callback = ^{
    [weakSelf
        abortPendingRequestWithReason:payments::JourneyLogger::
                                          ABORT_REASON_ABORTED_BY_MERCHANT
                         errorMessage:@"The website has aborted the payment"
                             callback:cancellationCallback];
  };

  [self dismissPaymentRequestUIWithCallback:^{
    [weakSelf displayErrorWithCallback:callback];
  }];

  return YES;
}

- (BOOL)handleCanMakePayment:(const base::DictionaryValue&)message {
  std::string errorMessage;
  payments::PaymentRequest* paymentRequest =
      [self paymentRequestFromMessage:message errorMessage:&errorMessage];
  if (!paymentRequest) {
    LOG(ERROR) << errorMessage;
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorName:kInvalidStateError
                                    errorMessage:base::SysUTF8ToNSString(
                                                     errorMessage)
                               completionHandler:nil];
    return YES;
  }

  if (paymentRequest->state() != payments::PaymentRequest::State::CREATED) {
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorName:kInvalidStateError
                                    errorMessage:@"Cannot query payment request"
                               completionHandler:nil];
    return YES;
  }

  if (![self webStateContentIsSecureHTML]) {
    [_paymentRequestJsManager resolveCanMakePaymentPromiseWithValue:NO
                                                  completionHandler:nil];
    paymentRequest->journey_logger().SetCanMakePaymentValue(false);
    return YES;
  }

  BOOL canMakePayment = paymentRequest->CanMakePayment();

  payments::CanMakePaymentQuery* canMakePaymentQuery =
      IOSCanMakePaymentQueryFactory::GetForBrowserState(
          _browserState->GetOriginalChromeBrowserState());
  DCHECK(canMakePaymentQuery);
  // iOS PaymentRequest does not support iframes or origin trials.
  if (canMakePaymentQuery->CanQuery(
          GURL(url_formatter::FormatUrlForSecurityDisplay(
              _activeWebState->GetLastCommittedURL())),
          GURL(url_formatter::FormatUrlForSecurityDisplay(
              _activeWebState->GetLastCommittedURL())),
          paymentRequest->stringified_method_data(),
          /*per_method_quota=*/false)) {
    // canMakePayment should return false if user has not allowed canMakePayment
    // to return a truthful value.
    canMakePayment &=
        _browserState->GetPrefs()->GetBoolean(payments::kCanMakePaymentEnabled);

    [_paymentRequestJsManager
        resolveCanMakePaymentPromiseWithValue:canMakePayment
                            completionHandler:nil];
    paymentRequest->journey_logger().SetCanMakePaymentValue(canMakePayment);
    // TODO(crbug.com/602666): Warn on console if origin is localhost or file.
  } else {
    [_paymentRequestJsManager
        rejectCanMakePaymentPromiseWithErrorName:kNotAllowedError
                                    errorMessage:
                                        @"Not allowed to check whether can "
                                        @"make payment"
                               completionHandler:nil];
  }
  return YES;
}

- (BOOL)displayErrorThenCancelRequest {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE) {
    return YES;
  }

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    [weakSelf abortPendingRequestWithReason:payments::JourneyLogger::
                                                ABORT_REASON_ABORTED_BY_USER
                               errorMessage:kCancelErrorMessage
                                   callback:nil];
  };

  [self dismissPaymentRequestUIWithCallback:^{
    [weakSelf displayErrorWithCallback:callback];
  }];

  return YES;
}

- (BOOL)doResponseComplete {
  base::DictionaryValue command;
  command.SetString("result", "unknown");
  return [self handleResponseComplete:command];
}

- (BOOL)handleResponseComplete:(const base::DictionaryValue&)message {
  if (!_pendingPaymentRequest)
    return YES;

  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  std::string result;
  if (!message.GetString("result", &result)) {
    LOG(ERROR) << "JS message parameter 'result' is missing";
    return NO;
  }

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlock callback = ^{
    weakSelf.pendingPaymentRequest = nullptr;
    [weakSelf.paymentRequestJsManager
        resolveResponsePromiseWithCompletionHandler:nil];
  };

  // Display UI indicating failure if the value of |result| is "fail".
  if (result == "fail") {
    [self dismissPaymentRequestUIWithCallback:^{
      [weakSelf displayErrorWithCallback:callback];
    }];
  } else {
    _pendingPaymentRequest->journey_logger().SetCompleted();
    _pendingPaymentRequest->set_updating(false);
    _pendingPaymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
    _pendingPaymentRequest->RecordUseStats();
    _pendingPaymentRequest->GetPrefService()->SetBoolean(
        payments::kPaymentsFirstTransactionCompleted, true);
    _pendingPaymentRequest->journey_logger().RecordTransactionAmount(
        _pendingPaymentRequest->payment_details().total->amount->currency,
        _pendingPaymentRequest->payment_details().total->amount->value,
        true /*completed*/);
    [self dismissPaymentRequestUIWithCallback:callback];
  }

  return YES;
}

- (BOOL)handleSetPendingRequestUpdating:(const base::DictionaryValue&)message {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE ||
      _pendingPaymentRequest->updating()) {
    return YES;
  }

  bool updating;
  if (!message.GetBoolean("updating", &updating)) {
    LOG(ERROR) << "JS message parameter 'updating' is missing";
    return NO;
  }

  _pendingPaymentRequest->set_updating(updating);
  return YES;
}

- (BOOL)handleUpdatePaymentDetails:(const base::DictionaryValue&)message {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE ||
      !_pendingPaymentRequest->updating()) {
    return YES;
  }

  [_unblockEventQueueTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  const base::DictionaryValue* paymentDetailsData = nullptr;
  payments::PaymentDetails paymentDetails;
  if (!message.GetDictionary("payment_details", &paymentDetailsData)) {
    LOG(ERROR) << "JS message parameter 'payment_details' is missing";
    return NO;
  }
  if (!paymentDetails.FromDictionaryValue(*paymentDetailsData,
                                          /*requires_total=*/false)) {
    LOG(ERROR) << "Cannot create payment details";
    return NO;
  }
  std::string errorMessage;
  if (!payments::ValidatePaymentDetails(paymentDetails, &errorMessage)) {
    LOG(ERROR) << errorMessage;
    return NO;
  }

  _pendingPaymentRequest->journey_logger().RecordTransactionAmount(
      _pendingPaymentRequest->payment_details().total->amount->currency,
      _pendingPaymentRequest->payment_details().total->amount->value,
      false /*completed*/);

  [_paymentRequestCoordinator updatePaymentDetails:paymentDetails];

  _pendingPaymentRequest->set_updating(false);

  return YES;
}

- (void)setUnblockEventQueueTimer {
  _unblockEventQueueTimer =
      [NSTimer scheduledTimerWithTimeInterval:kNoopInterval
                                       target:_paymentRequestJsManager
                                     selector:@selector(executeNoop)
                                     userInfo:nil
                                      repeats:YES];
}

- (void)setPaymentResponseTimeoutTimer {
  _paymentResponseTimeoutTimer =
      [NSTimer scheduledTimerWithTimeInterval:kTimeoutInterval
                                       target:self
                                     selector:@selector(doResponseComplete)
                                     userInfo:nil
                                      repeats:NO];
}

- (void)setUpdateEventTimeoutTimer {
  _updateEventTimeoutTimer = [NSTimer
      scheduledTimerWithTimeInterval:kTimeoutInterval
                              target:self
                            selector:@selector(displayErrorThenCancelRequest)
                            userInfo:nil
                             repeats:NO];
}

- (void)dismissPaymentRequestUIWithCallback:(ProceduralBlock)callback {
  [_unblockEventQueueTimer invalidate];
  [_paymentResponseTimeoutTimer invalidate];
  [_updateEventTimeoutTimer invalidate];

  [self resetIOSPaymentInstrumentLauncherDelegate];

  __weak PaymentRequestManager* weakSelf = self;
  [_paymentRequestCoordinator stopWithCompletion:^{
    weakSelf.paymentRequestCoordinator = nil;
    if (callback)
      callback();
  }];
}

- (BOOL)webStateContentIsSecureHTML {
  if (!_activeWebState) {
    return NO;
  }

  if (!self.locationBarModel) {
    return NO;
  }

  if (!_activeWebState->ContentIsHTML()) {
    LOG(ERROR) << "Not a web view with HTML.";
    return NO;
  }

  const GURL lastCommittedURL = _activeWebState->GetLastCommittedURL();

  if (!web::IsOriginSecure(lastCommittedURL) ||
      lastCommittedURL.scheme() == url::kDataScheme) {
    DLOG(ERROR) << "Not in a secure context.";
    return NO;
  }

  if (!security_state::IsSchemeCryptographic(lastCommittedURL) &&
      !security_state::IsOriginLocalhostOrFile(lastCommittedURL)) {
    DLOG(ERROR) << "Not localhost, or with file or cryptographic scheme.";
    return NO;
  }

  // If the scheme is cryptographic, the SSL certificate must also be valid.
  return !security_state::IsSchemeCryptographic(lastCommittedURL) ||
         security_state::IsSslCertificateValid(
             self.locationBarModel->GetSecurityLevel());
}

#pragma mark - PaymentRequestUIDelegate

- (void)paymentRequestDidFetchPaymentMethods:
    (payments::PaymentRequest*)paymentRequest {
  [_paymentRequestCoordinator setPending:NO];
  [_paymentRequestCoordinator setCancellable:YES];
}

- (void)
       paymentRequest:(payments::PaymentRequest*)paymentRequest
requestFullCreditCard:(const autofill::CreditCard&)creditCard
       resultDelegate:
           (base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>)
               delegate {
  [_paymentRequestCoordinator requestFullCreditCard:creditCard
                                     resultDelegate:delegate];
}

- (void)paymentInstrument:(payments::IOSPaymentInstrument*)paymentInstrument
    launchAppWithUniversalLink:(GURL)universalLink
            instrumentDelegate:(payments::PaymentApp::Delegate*)delegate {
  DCHECK(_pendingPaymentRequest);
  DCHECK(_activeWebState);

  [_paymentRequestCoordinator setPending:YES];
  [_paymentRequestCoordinator setCancellable:YES];

  payments::IOSPaymentInstrumentLauncher* paymentAppLauncher =
      payments::IOSPaymentInstrumentLauncherFactory::GetForBrowserState(
          _browserState->GetOriginalChromeBrowserState());
  DCHECK(paymentAppLauncher);
  if (!paymentAppLauncher->LaunchIOSPaymentInstrument(
          _pendingPaymentRequest, _activeWebState, universalLink, delegate)) {
    [_paymentRequestCoordinator setPending:NO];
    [_paymentRequestCoordinator setCancellable:YES];
  }
}

#pragma mark - PaymentRequestCoordinatorDelegate methods

- (void)paymentRequestCoordinatorDidConfirm:
    (PaymentRequestCoordinator*)coordinator {
  DCHECK(coordinator.paymentRequest->selected_payment_method());

  coordinator.paymentRequest->journey_logger().SetEventOccurred(
      payments::JourneyLogger::EVENT_PAY_CLICKED);
  coordinator.paymentRequest->journey_logger().SetEventOccurred(
      coordinator.paymentRequest->selected_payment_method()->type() ==
              payments::PaymentApp::Type::AUTOFILL
          ? payments::JourneyLogger::EVENT_SELECTED_CREDIT_CARD
          : payments::JourneyLogger::EVENT_SELECTED_OTHER);

  coordinator.paymentRequest->InvokePaymentApp(self);
}

- (void)paymentRequestCoordinatorDidCancel:
    (PaymentRequestCoordinator*)coordinator {
  [self dismissUIAndAbortPendingRequestWithReason:
            payments::JourneyLogger::ABORT_REASON_ABORTED_BY_USER
                                     errorMessage:kCancelErrorMessage
                                         callback:nil];
}

- (void)paymentRequestCoordinatorDidSelectSettings:
    (PaymentRequestCoordinator*)coordinator {
  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlockWithBool callback = ^(BOOL) {
    [weakSelf.dispatcher
        showAutofillSettingsFromViewController:_baseViewController];
  };

  [self dismissUIAndAbortPendingRequestWithReason:
            payments::JourneyLogger::ABORT_REASON_ABORTED_BY_USER
                                     errorMessage:kCancelErrorMessage
                                         callback:callback];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didReceiveFullMethodName:(const std::string&)methodName
               stringifiedDetails:(const std::string&)stringifiedDetails {
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
         didSelectShippingAddress:
             (const autofill::AutofillProfile&)shippingAddress {
  if (coordinator.paymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE ||
      coordinator.paymentRequest->updating()) {
    return;
  }

  payments::mojom::PaymentAddressPtr address =
      payments::data_util::GetPaymentAddressFromAutofillProfile(
          shippingAddress, coordinator.paymentRequest->GetApplicationLocale());
  [_paymentRequestJsManager updateShippingAddress:*address
                                completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

- (void)paymentRequestCoordinator:(PaymentRequestCoordinator*)coordinator
          didSelectShippingOption:
              (const payments::PaymentShippingOption&)shippingOption {
  if (coordinator.paymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE ||
      coordinator.paymentRequest->updating()) {
    return;
  }

  [_paymentRequestJsManager updateShippingOption:shippingOption
                               completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setUpdateEventTimeoutTimer];
}

#pragma mark - PaymentRequestErrorCoordinatorDelegate

- (void)paymentRequestErrorCoordinatorDidDismiss:
    (PaymentRequestErrorCoordinator*)coordinator {
  ProceduralBlock callback = coordinator.callback;

  [_paymentRequestErrorCoordinator stop];
  _paymentRequestErrorCoordinator = nil;

  if (callback)
    callback();
}

#pragma mark - PaymentResponseHelperConsumer methods

- (void)paymentResponseHelperDidReceivePaymentMethodDetails {
  [_paymentRequestCoordinator setPending:YES];
}

- (void)paymentResponseHelperDidFailToReceivePaymentMethodDetails {
  [_paymentRequestCoordinator setPending:NO];
  [_paymentRequestCoordinator setCancellable:YES];
}

- (void)paymentResponseHelperDidCompleteWithPaymentResponse:
    (const payments::PaymentResponse&)paymentResponse {
  if (!_pendingPaymentRequest ||
      _pendingPaymentRequest->state() !=
          payments::PaymentRequest::State::INTERACTIVE ||
      _pendingPaymentRequest->updating()) {
    return;
  }

  [_paymentRequestCoordinator setCancellable:NO];

  [_paymentRequestJsManager
      resolveRequestPromiseWithPaymentResponse:paymentResponse
                             completionHandler:nil];
  [self setUnblockEventQueueTimer];
  [self setPaymentResponseTimeoutTimer];
}

#pragma mark - CRWWebStateObserver methods

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  // Ignore navigations within the same document, e.g., history.pushState().
  if (navigation->IsSameDocument())
    return;

  DCHECK_EQ(_activeWebState, webState);
  payments::JourneyLogger::AbortReason abortReason =
      navigation->IsRendererInitiated()
          ? payments::JourneyLogger::ABORT_REASON_MERCHANT_NAVIGATION
          : payments::JourneyLogger::ABORT_REASON_USER_NAVIGATION;

  __weak PaymentRequestManager* weakSelf = self;
  ProceduralBlockWithBool callback = ^(BOOL) {
    for (const auto& paymentRequest :
         weakSelf.paymentRequestCache->GetPaymentRequests(
             weakSelf.activeWebState)) {
      if (paymentRequest->state() != payments::PaymentRequest::State::CLOSED) {
        paymentRequest->journey_logger().SetAborted(abortReason);
        paymentRequest->set_updating(false);
        paymentRequest->set_state(payments::PaymentRequest::State::CLOSED);
      }
    }
    // The lifetime of a PaymentRequest is tied to the WebState it is associated
    // with and the current URL. Therefore, PaymentRequest instances should get
    // destroyed when the user navigates to a URL.
    weakSelf.paymentRequestCache->ClearPaymentRequests(weakSelf.activeWebState);
  };

  // Abort any pending request.
  if (_pendingPaymentRequest) {
    [self dismissUIAndAbortPendingRequestWithReason:abortReason
                                       errorMessage:kCancelErrorMessage
                                           callback:callback];
  } else {
    callback(YES);
  }

  // Set the JS isContextSecure global variable at the earliest opportunity.
  [_paymentRequestJsManager
       setContextSecure:(web::IsOriginSecure(
                             _activeWebState->GetLastCommittedURL()) &&
                         _activeWebState->GetLastCommittedURL().scheme() !=
                             url::kDataScheme)
      completionHandler:nil];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_activeWebState, webState);

  // This unregister the observer from WebState.
  self.activeWebState = nullptr;
}

#pragma mark - Helper methods

- (void)displayErrorWithCallback:(ProceduralBlock)callback {
  _paymentRequestErrorCoordinator = [[PaymentRequestErrorCoordinator alloc]
      initWithBaseViewController:_baseViewController];
  [_paymentRequestErrorCoordinator setCallback:callback];
  [_paymentRequestErrorCoordinator setDelegate:self];

  [_paymentRequestErrorCoordinator start];
}

- (payments::PaymentRequest*)paymentRequestWithId:
    (std::string)paymentRequestId {
  const payments::PaymentRequestCache::PaymentRequestSet& paymentRequests =
      _paymentRequestCache->GetPaymentRequests(_activeWebState);
  const auto found = std::find_if(
      paymentRequests.begin(), paymentRequests.end(),
      [&paymentRequestId](
          const std::unique_ptr<payments::PaymentRequest>& request) {
        return request.get()->web_payment_request().payment_request_id ==
               paymentRequestId;
      });
  return found != paymentRequests.end() ? found->get() : nullptr;
}

@end

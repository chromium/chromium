// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"

#import <memory>
#import <string>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/json/string_escape.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/chrome/browser/autofill/model/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_client.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/form_observer_helper.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/security_alert_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using base::UmaHistogramEnumeration;
using password_manager::FillData;

namespace {

// Delay before queueing an utterance. It is required to ensure that standard
// announcements have already started and thus won't be interrupted.
constexpr base::TimeDelta kA11yAnnouncementQueueDelay = base::Seconds(1);

}  // namespace

@interface ManualFillInjectionHandler ()<FormActivityObserver>

// The object in charge of listening to form events and reporting back.
@property(nonatomic, strong) FormObserverHelper* formHelper;

// Interface for `reauthenticationModule`, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// The WebStateList with the relevant active web state for the injection.
@property(nonatomic, assign) WebStateList* webStateList;

// YES if the last focused element is secure within its web frame. To be secure
// means the web is HTTPS and the URL is trusted.
@property(nonatomic, assign, getter=isLastFocusedElementSecure)
    BOOL lastFocusedElementSecure;

// YES if the last focused element is a password field.
@property(nonatomic, assign, getter=isLastFocusedElementPasswordField)
    BOOL lastFocusedElementPasswordField;

// The last seen form ID with focus activity.
@property(nonatomic, assign)
    autofill::FormRendererId lastFocusedElementFormIdentifier;

// The last seen frame ID with focus activity.
@property(nonatomic, assign) std::string lastFocusedElementFrameIdentifier;

// The last seen focused element identifier.
@property(nonatomic, assign)
    autofill::FieldRendererId lastFocusedElementUniqueId;

// Used to present alerts.
@property(nonatomic, weak) id<SecurityAlertCommands> securityAlertHandler;

// Used to entirely fill the current form with a suggestion.
@property(nonatomic, weak) id<FormSuggestionClient> formSuggestionClient;

@end

@implementation ManualFillInjectionHandler

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule
      formSuggestionClient:(id<FormSuggestionClient>)formSuggestionClient {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _securityAlertHandler = securityAlertHandler;
    _formHelper =
        [[FormObserverHelper alloc] initWithWebStateList:webStateList];
    _formHelper.delegate = self;
    _reauthenticationModule = reauthenticationModule;
    _formSuggestionClient = formSuggestionClient;
  }
  return self;
}

#pragma mark - Getters

// Returns the reauthentication module, which can be an override for testing
// purposes.
- (ReauthenticationModule*)reauthenticationModule {
  id<ReauthenticationProtocol> overrideModule =
      ScopedFormInputAccessoryReauthModuleOverride::Get();
  return overrideModule ? overrideModule : _reauthenticationModule;
}

#pragma mark - ManualFillContentInjector

- (BOOL)canUserInjectInPasswordField:(BOOL)passwordField
                       requiresHTTPS:(BOOL)requiresHTTPS {
  if (passwordField && ![self isLastFocusedElementPasswordField]) {
    NSString* alertBody = l10n_util::GetNSString(
        IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_PASSWORD_BODY);
    [self.securityAlertHandler presentSecurityWarningAlertWithText:alertBody];
    return NO;
  }
  if (requiresHTTPS && ![self isLastFocusedElementSecure]) {
    NSString* alertBody =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_GENERIC_BODY);
    [self.securityAlertHandler presentSecurityWarningAlertWithText:alertBody];
    return NO;
  }
  return YES;
}

- (void)userDidPickContent:(NSString*)content
             passwordField:(BOOL)passwordField
             requiresHTTPS:(BOOL)requiresHTTPS {
  if (passwordField) {
    UmaHistogramEnumeration("IOS.Reauth.Password.ManualFallback",
                            ReauthenticationEvent::kAttempt);
  }

  if ([self canUserInjectInPasswordField:passwordField
                           requiresHTTPS:requiresHTTPS]) {
    if (!passwordField) {
      [self fillLastSelectedFieldWithString:content];
      return;
    }

    if ([self.reauthenticationModule canAttemptReauth]) {
      NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
      __weak __typeof(self) weakSelf = self;
      auto completionHandler = ^(ReauthenticationResult result) {
        if (result != ReauthenticationResult::kFailure) {
          UmaHistogramEnumeration("IOS.Reauth.Password.ManualFallback",
                                  ReauthenticationEvent::kSuccess);
          [weakSelf fillLastSelectedFieldWithString:content];
        } else {
          UmaHistogramEnumeration("IOS.Reauth.Password.ManualFallback",
                                  ReauthenticationEvent::kFailure);
        }
      };

      [self.reauthenticationModule
          attemptReauthWithLocalizedReason:reason
                      canReusePreviousAuth:YES
                                   handler:completionHandler];
    } else {
      UmaHistogramEnumeration("IOS.Reauth.Password.ManualFallback",
                              ReauthenticationEvent::kMissingPasscode);
      [self fillLastSelectedFieldWithString:content];
    }
  }
}

- (void)autofillFormWithCredential:(ManualFillCredential*)credential
                      shouldReauth:(BOOL)shouldReauth {
  if (shouldReauth && [self.reauthenticationModule canAttemptReauth]) {
    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    auto completionHandler = ^(ReauthenticationResult result) {
      if (result != ReauthenticationResult::kFailure) {
        [self fillFormWithCredential:credential];
      }
    };

    [self.reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self fillFormWithCredential:credential];
  }
}

- (void)autofillFormWithSuggestion:(FormSuggestion*)formSuggestion
                           atIndex:(NSInteger)index {
  [self.formSuggestionClient didSelectSuggestion:formSuggestion atIndex:index];
}

- (BOOL)isActiveFormAPasswordForm {
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return NO;
  }

  PasswordTabHelper* tabHelper =
      PasswordTabHelper::FromWebState(activeWebState);
  if (!tabHelper) {
    return NO;
  }

  const password_manager::PasswordForm* observedForm =
      [self currentPasswordFormFromWebState:activeWebState tabHelper:tabHelper];

  return observedForm != nullptr;
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const autofill::FormActivityParams&)params
                    inFrame:(web::WebFrame*)frame {
  if (params.type != "focus") {
    return;
  }
  self.lastFocusedElementSecure =
      autofill::IsContextSecureForWebState(webState);
  self.lastFocusedElementPasswordField = params.field_type == "password";
  self.lastFocusedElementUniqueId = params.field_renderer_id;
  DCHECK(frame);
  self.lastFocusedElementFrameIdentifier = frame->GetFrameId();
  self.lastFocusedElementFormIdentifier = params.form_renderer_id;
  const GURL frameSecureOrigin = frame->GetSecurityOrigin();
  if (!frameSecureOrigin.SchemeIsCryptographic()) {
    self.lastFocusedElementSecure = NO;
  }
}

#pragma mark - Private

// Returns the last focused web frame associated with the given `webState`.
- (web::WebFrame*)activeWebFrameFromWebState:(web::WebState*)webState {
  autofill::AutofillJavaScriptFeature* feature =
      autofill::AutofillJavaScriptFeature::GetInstance();

  return feature->GetWebFramesManager(webState)->GetFrameWithId(
      self.lastFocusedElementFrameIdentifier);
}

// Injects the passed string to the active field and jumps to the next field.
- (void)fillLastSelectedFieldWithString:(NSString*)string {
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  web::WebFrame* activeWebFrame =
      [self activeWebFrameFromWebState:activeWebState];
  if (!activeWebFrame) {
    return;
  }

  base::Value::Dict data;
  data.Set("renderer_id",
           static_cast<int>(self.lastFocusedElementUniqueId.value()));
  data.Set("value", base::SysNSStringToUTF16(string));
  autofill::AutofillJavaScriptFeature::GetInstance()->FillActiveFormField(
      activeWebFrame, std::move(data), base::BindOnce(^(BOOL success) {
        [self jumpToNextField];
      }));
}

// Attempts to jump to the next field in the current form.
- (void)jumpToNextField {
  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.webState = self.webStateList->GetActiveWebState();
  [handler setLastFocusFormActivityWebFrameID:
               base::SysUTF8ToNSString(self.lastFocusedElementFrameIdentifier)];
  [handler selectNextElementWithoutButtonPress];
}

// Fills the current form with the given `credential`. Only works if the current
// form is a password form, otherwise it's a no-op.
- (void)fillFormWithCredential:(ManualFillCredential*)credential {
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }

  PasswordTabHelper* tabHelper =
      PasswordTabHelper::FromWebState(activeWebState);
  if (!tabHelper) {
    return;
  }

  const password_manager::PasswordForm* observedForm =
      [self currentPasswordFormFromWebState:activeWebState tabHelper:tabHelper];
  if (!observedForm) {
    return;
  }

  FillData fillData = [self makeFillDataForCredential:credential
                                          currentForm:*observedForm];
  SharedPasswordController* sharedPasswordController =
      tabHelper->GetSharedPasswordController();
  [self fillFormWithFillData:fillData
                    webState:activeWebState
                  formHelper:sharedPasswordController.formHelper];
}

// Returns the observed parsed password form to which the last focused field
// belongs. Might return `nil` if the PasswordManager doesn't observe any parsed
// form.
- (const password_manager::PasswordForm*)
    currentPasswordFormFromWebState:(web::WebState*)webState
                          tabHelper:(PasswordTabHelper*)tabHelper {
  password_manager::PasswordManager* passwordManager =
      tabHelper->GetPasswordManager();
  CHECK(passwordManager);

  web::WebFrame* frame = [self activeWebFrameFromWebState:webState];
  if (!frame) {
    return nil;
  }

  password_manager::PasswordManagerDriver* driver =
      IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(webState, frame);
  CHECK(driver);

  return passwordManager->GetParsedObservedForm(
      driver, self.lastFocusedElementUniqueId);
}

// Creates and returns FillData for the given `credential`.
- (FillData)makeFillDataForCredential:(ManualFillCredential*)credential
                          currentForm:(const password_manager::PasswordForm&)
                                          currentForm {
  FillData fillData;
  fillData.origin = credential.URL;
  fillData.form_id = self.lastFocusedElementFormIdentifier;
  fillData.username_element_id = currentForm.username_element_renderer_id;
  fillData.username_value = base::SysNSStringToUTF16(credential.username);
  fillData.password_element_id = currentForm.password_element_renderer_id;
  fillData.password_value = base::SysNSStringToUTF16(credential.password);

  return fillData;
}

// Uses `fillData` to fill a password form.
- (void)fillFormWithFillData:(FillData)fillData
                    webState:(web::WebState*)webState
                  formHelper:(PasswordFormHelper*)formHelper {
  web::WebFrame* activeWebFrame = [self activeWebFrameFromWebState:webState];
  if (!activeWebFrame) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [formHelper fillPasswordFormWithFillData:fillData
                                   inFrame:activeWebFrame
                          triggeredOnField:self.lastFocusedElementUniqueId
                         completionHandler:^(BOOL success) {
                           if (success) {
                             [weakSelf announceFormWasFilled];
                           }
                         }];
}

// Announces by VoiceOver that the form was filled.
- (void)announceFormWasFilled {
  if (!UIAccessibilityIsVoiceOverRunning()) {
    return;
  }

  // The announcement is done asynchronously with a certain delay to make sure
  // it is not interrupted by (almost) immediate standard announcements.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    kA11yAnnouncementQueueDelay.InNanoseconds()),
      dispatch_get_main_queue(), ^{
        // Use the queue flag to preserve standard announcements, they are
        // conveyed first and then announce this message. This is a tradeoff as
        // there is no control over the standard utterances (they are
        // interrupting) and it is not desirable to interrupt them. Hence
        // acceptance announcement is done after standard ones (which takes
        // seconds).
        NSAttributedString* message = [[NSAttributedString alloc]
            initWithString:l10n_util::GetNSString(
                               IDS_AUTOFILL_A11Y_ANNOUNCE_FILLED_FORM)
                attributes:@{
                  UIAccessibilitySpeechAttributeQueueAnnouncement : @YES
                }];
        UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                        message);
      });
}

@end

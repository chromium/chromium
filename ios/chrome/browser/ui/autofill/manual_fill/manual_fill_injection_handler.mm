// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"

#import <memory>
#import <string>
#import <vector>

#import "base/functional/bind.h"
#import "base/json/string_escape.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/security_alert_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/form_observer_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UmaHistogramEnumeration;

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

// The last seen frame ID with focus activity.
@property(nonatomic, assign) std::string lastFocusedElementFrameIdentifier;

// The last seen focused element identifier.
@property(nonatomic, assign)
    autofill::FieldRendererId lastFocusedElementUniqueId;

// Used to present alerts.
@property(nonatomic, weak) id<SecurityAlertCommands> securityAlertHandler;

@end

@implementation ManualFillInjectionHandler

- (instancetype)
      initWithWebStateList:(WebStateList*)webStateList
      securityAlertHandler:(id<SecurityAlertCommands>)securityAlertHandler
    reauthenticationModule:(ReauthenticationModule*)reauthenticationModule {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _securityAlertHandler = securityAlertHandler;
    _formHelper =
        [[FormObserverHelper alloc] initWithWebStateList:webStateList];
    _formHelper.delegate = self;
    _reauthenticationModule = reauthenticationModule;
  }
  return self;
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
  self.lastFocusedElementUniqueId = params.unique_field_id;
  DCHECK(frame);
  self.lastFocusedElementFrameIdentifier = frame->GetFrameId();
  const GURL frameSecureOrigin = frame->GetSecurityOrigin();
  if (!frameSecureOrigin.SchemeIsCryptographic()) {
    self.lastFocusedElementSecure = NO;
  }
}

#pragma mark - Private

// Injects the passed string to the active field and jumps to the next field.
- (void)fillLastSelectedFieldWithString:(NSString*)string {
  web::WebState* activeWebState = self.webStateList->GetActiveWebState();
  if (!activeWebState) {
    return;
  }
  web::WebFrame* activeWebFrame = web::GetWebFrameWithId(
      activeWebState, self.lastFocusedElementFrameIdentifier);
  if (!activeWebFrame) {
    return;
  }

  base::Value::Dict data;
  data.Set("unique_renderer_id",
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

@end

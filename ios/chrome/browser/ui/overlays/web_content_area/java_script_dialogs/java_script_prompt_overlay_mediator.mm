// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_prompt_overlay_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_prompt_overlay.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_action.h"
#import "ios/chrome/browser/ui/alert_view_controller/alert_view_controller.h"
#import "ios/chrome/browser/ui/elements/text_field_configuration.h"
#import "ios/chrome/browser/ui/overlays/common/alerts/alert_overlay_mediator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_dialog_blocking_action.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_overlay_mediator_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kJavaScriptPromptTextFieldAccessibiltyIdentifier =
    @"JavaScriptPromptTextFieldAccessibiltyIdentifier";

@interface JavaScriptPromptOverlayMediator ()
@property(nonatomic, readonly) OverlayRequest* request;
@property(nonatomic, readonly) JavaScriptPromptOverlayRequestConfig* config;

// Sets the OverlayResponse using the user input |textInput| from the prompt UI.
- (void)setPromptResponse:(NSString*)textInput;
@end

@implementation JavaScriptPromptOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if (self = [super init]) {
    _request = request;
    DCHECK(_request);
    // Verify that the request is configured for JavaScript prompts.
    DCHECK(_request->GetConfig<JavaScriptPromptOverlayRequestConfig>());
  }
  return self;
}

#pragma mark - Accessors

- (JavaScriptPromptOverlayRequestConfig*)config {
  return self.request->GetConfig<JavaScriptPromptOverlayRequestConfig>();
}

#pragma mark - Response helpers

- (void)setPromptResponse:(NSString*)textInput {
  self.request->set_response(
      OverlayResponse::CreateWithInfo<JavaScriptPromptOverlayResponseInfo>(
          base::SysNSStringToUTF8(textInput)));
}

@end

@implementation JavaScriptPromptOverlayMediator (Subclassing)

- (NSString*)alertTitle {
  return GetJavaScriptDialogTitle(self.config->source(),
                                  self.config->message());
}

- (NSString*)alertMessage {
  return GetJavaScriptDialogMessage(self.config->source(),
                                    self.config->message());
}

- (NSArray<TextFieldConfiguration*>*)alertTextFieldConfigurations {
  NSString* defaultPromptValue =
      base::SysUTF8ToNSString(self.config->default_prompt_value());
  return @[ [[TextFieldConfiguration alloc]
                 initWithText:defaultPromptValue
                  placeholder:nil
      accessibilityIdentifier:kJavaScriptPromptTextFieldAccessibiltyIdentifier
              secureTextEntry:NO] ];
}

- (NSArray<AlertAction*>*)alertActions {
  __weak __typeof__(self) weakSelf = self;
  NSArray* actions = @[
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                           style:UIAlertActionStyleDefault
                         handler:^(AlertAction* action) {
                           __typeof__(self) strongSelf = weakSelf;
                           NSString* input = [strongSelf.dataSource
                               textFieldInputForMediator:strongSelf
                                          textFieldIndex:0];
                           [strongSelf setPromptResponse:input ? input : @""];
                           [strongSelf.delegate
                               stopDialogForMediator:strongSelf];
                         }],
    [AlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                           style:UIAlertActionStyleCancel
                         handler:^(AlertAction* action) {
                           [weakSelf.delegate stopDialogForMediator:weakSelf];
                         }],
  ];
  AlertAction* blockingAction =
      GetBlockingAlertAction(self, self.config->source());
  if (blockingAction)
    actions = [actions arrayByAddingObject:blockingAction];
  return actions;
}

@end

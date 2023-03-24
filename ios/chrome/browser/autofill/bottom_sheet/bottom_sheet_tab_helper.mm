// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_tab_helper.h"

#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_java_script_feature.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/password_bottom_sheet_commands.h"
#import "ios/web/public/js_messaging/script_message.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BottomSheetTabHelper::~BottomSheetTabHelper() = default;

BottomSheetTabHelper::BottomSheetTabHelper(web::WebState* web_state) {
  web_state->AddObserver(this);
}

// Public methods

void BottomSheetTabHelper::SetPasswordBottomSheetHandler(
    id<PasswordBottomSheetCommands> handler) {
  handler_ = handler;
}

void BottomSheetTabHelper::OnFormMessageReceived(
    const web::ScriptMessage& message) {
  autofill::FormActivityParams params;
  if (handler_ && autofill::FormActivityParams::FromMessage(message, &params)) {
    [handler_ showPasswordBottomSheet:params];
  }
}

void BottomSheetTabHelper::AttachListeners(
    const std::vector<autofill::FieldRendererId>& renderer_ids,
    web::WebFrame* frame) {
  BottomSheetJavaScriptFeature::GetInstance()->AttachListeners(renderer_ids,
                                                               frame);
}

void BottomSheetTabHelper::DetachListenersAndRefocus(web::WebFrame* frame) {
  BottomSheetJavaScriptFeature::GetInstance()->DetachListenersAndRefocus(frame);
}

// WebStateObserver

void BottomSheetTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  handler_ = nil;
}

WEB_STATE_USER_DATA_KEY_IMPL(BottomSheetTabHelper)

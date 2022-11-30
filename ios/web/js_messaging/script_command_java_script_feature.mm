// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/script_command_java_script_feature.h"

#import <string>

#import "base/logging.h"
#import "base/no_destructor.h"
#import "base/values.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kScriptCommandHandlerName[] = "crwebinvoke";

const char kScriptMessageFrameIdKey[] = "crwFrameId";
const char kScriptMessageCommandDictKey[] = "crwCommand";
const char kScriptMessageCommandKey[] = "command";

}  // namespace

namespace web {

// static
ScriptCommandJavaScriptFeature* ScriptCommandJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ScriptCommandJavaScriptFeature> instance;
  return instance.get();
}

ScriptCommandJavaScriptFeature::ScriptCommandJavaScriptFeature()
    : JavaScriptFeature(ContentWorld::kPageContentWorld, {}) {}
ScriptCommandJavaScriptFeature::~ScriptCommandJavaScriptFeature() = default;

absl::optional<std::string>
ScriptCommandJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptCommandHandlerName;
}

void ScriptCommandJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& script_message) {
  if (!script_message.body() || !script_message.body()->is_dict()) {
    return;
  }

  web::WebFrame* sender_frame = nullptr;
  std::string* frame_id =
      script_message.body()->FindStringKey(kScriptMessageFrameIdKey);
  if (frame_id) {
    sender_frame = web::GetWebFrameWithId(web_state, *frame_id);
  }
  // Message must be associated with a current frame.
  if (!sender_frame) {
    DLOG(WARNING) << "Message from JS not handled due to no matching frame";
    return;
  }

  base::Value* crw_command_dict =
      script_message.body()->FindDictKey(kScriptMessageCommandDictKey);
  if (!crw_command_dict || !crw_command_dict->is_dict()) {
    DLOG(WARNING) << "JS message parameter not found: crwCommand";
    return;
  }

  std::string* command =
      crw_command_dict->FindStringKey(kScriptMessageCommandKey);
  if (!command) {
    DLOG(WARNING) << "JS message parameter not found: command";
    return;
  }

  WebStateImpl* web_state_impl = static_cast<WebStateImpl*>(web_state);
  if (!web_state_impl) {
    return;
  }

  BOOL user_interacting =
      [web_state_impl->GetWebController() isUserInteracting];
  absl::optional<GURL> request_url = script_message.request_url();
  GURL url = request_url ? request_url.value() : GURL();
  web_state_impl->OnScriptCommandReceived(*command, *crw_command_dict, url,
                                          user_interacting, sender_frame);
}

}  // namespace web

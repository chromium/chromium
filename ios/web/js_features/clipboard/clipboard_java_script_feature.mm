// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/clipboard/clipboard_java_script_feature.h"

#import "base/functional/bind.h"
#import "base/values.h"
#import "ios/web/js_features/clipboard/clipboard_constants.h"
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/script_message.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate.h"

namespace {
const char kScriptName[] = "clipboard";
}  // namespace

namespace web {

// static
ClipboardJavaScriptFeature* ClipboardJavaScriptFeature::GetInstance() {
  static base::NoDestructor<ClipboardJavaScriptFeature> instance;
  return instance.get();
}

ClipboardJavaScriptFeature::ClipboardJavaScriptFeature()
    : JavaScriptFeature(
          ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)},
          {java_script_features::GetBaseJavaScriptFeature()}) {}

ClipboardJavaScriptFeature::~ClipboardJavaScriptFeature() = default;

std::optional<std::string>
ClipboardJavaScriptFeature::GetScriptMessageHandlerName() const {
  return kScriptMessageHandlerName;
}

void ClipboardJavaScriptFeature::ScriptMessageReceived(
    WebState* web_state,
    const ScriptMessage& message) {
  const base::Value::Dict* body = message.body()->GetIfDict();
  if (!body) {
    return;
  }

  // In JavaScript, all numbers are doubles.
  std::optional<double> request_id_double = body->FindDouble(kRequestIdKey);
  if (!request_id_double) {
    return;
  }
  int request_id = static_cast<int>(*request_id_double);

  const std::string* frame_id = body->FindString(kFrameIdKey);
  if (!frame_id) {
    return;
  }

  WebFrame* web_frame =
      GetWebFramesManager(web_state)->GetFrameWithId(*frame_id);
  if (!web_frame) {
    return;
  }

  const std::string* command = body->FindString(kCommandKey);
  if (!command) {
    return;
  }

  // Requests are allowed by default.
  if (!web_state->GetDelegate()) {
    ResolveClipboardRequest(request_id, web_frame->AsWeakPtr(),
                            /* allowed= */ true);
    return;
  }

  // Request Clipboard access approval from the WebState's delegate.
  // It is safe to bind the callbacks to the singleton instance of
  // ClipboardJavaScriptFeature because it is never destroyed.
  base::OnceCallback<void(bool)> callback = base::BindOnce(
      &ClipboardJavaScriptFeature::ResolveClipboardRequest,
      base::Unretained(GetInstance()), request_id, web_frame->AsWeakPtr());

  // Clipboard write operations from JavaScript are evaluated by the same
  // policy framework as native "copy" actions. Similarly, "read" operations
  // are evaluated as "paste" actions. This approach is consistent with the
  // desktop implementation in
  // content/browser/renderer_host/clipboard_host_impl.cc.
  if (*command == kWriteCommand) {
    web_state->GetDelegate()->ShouldAllowCopy(web_state, std::move(callback));
  } else if (*command == kReadCommand) {
    web_state->GetDelegate()->ShouldAllowPaste(web_state, std::move(callback));
  }
}

void ClipboardJavaScriptFeature::ResolveClipboardRequest(
    int request_id,
    base::WeakPtr<WebFrame> web_frame,
    bool allowed) {
  if (!web_frame) {
    return;
  }

  base::Value::List parameters;
  parameters.Append(request_id);
  parameters.Append(allowed);
  CallJavaScriptFunction(web_frame.get(), "clipboard.resolveRequest",
                         parameters);
}

}  // namespace web

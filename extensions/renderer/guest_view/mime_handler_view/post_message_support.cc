// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/mime_handler_view/post_message_support.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#include "gin/arguments.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/interceptor.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace extensions {

namespace {

const char kPostMessageName[] = "postMessage";

// The gin-backed scriptable object which is exposed by the BrowserPlugin for
// PostMessageSupport. This currently only implements "postMessage".
class ScriptableObject : public gin::Wrappable<ScriptableObject>,
                         public gin::NamedPropertyInterceptor {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static v8::Local<v8::Object> Create(
      v8::Isolate* isolate,
      base::WeakPtr<PostMessageSupport> post_message_support) {
    ScriptableObject* scriptable_object =
        new ScriptableObject(isolate, post_message_support);
    return gin::CreateHandle(isolate, scriptable_object)
        .ToV8()
        .As<v8::Object>();
  }

  // gin::NamedPropertyInterceptor
  v8::Local<v8::Value> GetNamedProperty(
      v8::Isolate* isolate,
      const std::string& identifier) override {
    if (identifier == kPostMessageName) {
      if (post_message_function_template_.IsEmpty()) {
        post_message_function_template_.Reset(
            isolate,
            gin::CreateFunctionTemplate(
                isolate,
                base::BindRepeating(&PostMessageSupport::PostJavaScriptMessage,
                                    post_message_support_, isolate)));
      }
      v8::Local<v8::FunctionTemplate> function_template =
          v8::Local<v8::FunctionTemplate>::New(isolate,
                                               post_message_function_template_);
      v8::Local<v8::Function> function;
      if (function_template->GetFunction(isolate->GetCurrentContext())
              .ToLocal(&function))
        return function;
    }
    return v8::Local<v8::Value>();
  }

 private:
  ScriptableObject(v8::Isolate* isolate,
                   base::WeakPtr<PostMessageSupport> post_message_support)
      : gin::NamedPropertyInterceptor(isolate, this),
        post_message_support_(post_message_support) {}

  // gin::Wrappable
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override {
    return gin::Wrappable<ScriptableObject>::GetObjectTemplateBuilder(isolate)
        .AddNamedPropertyInterceptor();
  }

  base::WeakPtr<PostMessageSupport> post_message_support_;
  v8::Persistent<v8::FunctionTemplate> post_message_function_template_;
};

// static
gin::WrapperInfo ScriptableObject::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

PostMessageSupport::Delegate::Delegate()
    : post_message_support_(std::make_unique<PostMessageSupport>(this)) {}
PostMessageSupport::Delegate::~Delegate() = default;

// static
PostMessageSupport* PostMessageSupport::FromWebLocalFrame(
    blink::WebLocalFrame* web_local_frame) {
  // TODO(ekaramad): We shouldn't have this dependency here. After resolving
  // 659750 move implementation of Delegate::FromWebLocalFrame to MHVCM
  // instead.
  // If this is for an <iframe> or main frame navigation to PDF, then the
  // MHVCM is created before |web_local_frame| loads.
  auto* manager = MimeHandlerViewContainerManager::Get(
      content::RenderFrame::FromWebFrame(web_local_frame),
      false /* create_if_does_not_exist */);
  return manager ? manager->GetPostMessageSupport() : nullptr;
}

PostMessageSupport::PostMessageSupport(Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

PostMessageSupport::~PostMessageSupport() {
  DCHECK(delegate_);
}

v8::Local<v8::Object> PostMessageSupport::GetScriptableObject(
    v8::Isolate* isolate) {
  if (scriptable_object_.IsEmpty()) {
    v8::Local<v8::Object> object =
        ScriptableObject::Create(isolate, weak_factory_.GetWeakPtr());
    scriptable_object_.Reset(isolate, object);
  }
  return v8::Local<v8::Object>::New(isolate, scriptable_object_);
}

void PostMessageSupport::PostJavaScriptMessage(v8::Isolate* isolate,
                                               v8::Local<v8::Value> message) {
  if (!is_active_) {
    pending_messages_.push_back(v8::Global<v8::Value>(isolate, message));
    return;
  }

  auto* target_frame = delegate_->GetTargetFrame();
  if (!target_frame) {
    // |this| might be deleted at this point.
    return;
  }

  v8::Context::Scope context_scope(
      delegate_->GetSourceFrame()->MainWorldScriptContext());
  v8::Local<v8::Object> target_window_proxy =
      target_frame->GlobalProxy(isolate);
  gin::Dictionary window_object(isolate, target_window_proxy);
  v8::Local<v8::Function> post_message;
  if (!window_object.Get(std::string(kPostMessageName), &post_message))
    return;

  v8::Local<v8::Value> args[] = {
      message,
      // Post the message to any domain inside the browser plugin. The embedder
      // should already know what is embedded.
      gin::StringToV8(isolate, "*")};
  delegate_->GetSourceFrame()->CallFunctionEvenIfScriptDisabled(
      post_message.As<v8::Function>(), target_window_proxy, std::size(args),
      args);
}

void PostMessageSupport::PostMessageFromValue(const base::Value& message) {
  auto* frame = delegate_->GetSourceFrame();
  v8::Isolate* isolate = frame->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(frame->MainWorldScriptContext());
  PostJavaScriptMessage(isolate, content::V8ValueConverter::Create()->ToV8Value(
                                     message, frame->MainWorldScriptContext()));
}

void PostMessageSupport::SetActive() {
  DCHECK(!is_active_);
  is_active_ = true;
  if (pending_messages_.empty())
    return;

  // Now that the guest has loaded, flush any unsent messages.
  auto* source = delegate_->GetSourceFrame();
  v8::Isolate* isolate = source->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(source->MainWorldScriptContext());
  for (const auto& pending_message : pending_messages_)
    PostJavaScriptMessage(isolate,
                          v8::Local<v8::Value>::New(isolate, pending_message));

  pending_messages_.clear();
}

}  // namespace extensions

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dom_activity_logger.h"

#include <memory>
#include <utility>

#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "v8/include/v8-isolate.h"

using blink::WebString;
using blink::WebURL;

namespace extensions {

namespace {

// Converts the given |v8_value| and appends it to the given |list|, if the
// conversion succeeds.
void AppendV8Value(const std::string& api_name,
                   const v8::Local<v8::Value>& v8_value,
                   base::Value::List& list) {
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  ActivityLogConverterStrategy strategy;
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);
  std::unique_ptr<base::Value> value(converter->FromV8Value(
      v8_value, v8::Isolate::GetCurrent()->GetCurrentContext()));

  if (value)
    list.Append(base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace

DOMActivityLogger::DOMActivityLogger(const std::string& extension_id)
    : extension_id_(extension_id) {}

DOMActivityLogger::~DOMActivityLogger() = default;

void DOMActivityLogger::AttachToWorld(int32_t world_id,
                                      const std::string& extension_id) {
  // If there is no logger registered for world_id, construct a new logger
  // and register it with world_id.
  if (!blink::HasDOMActivityLogger(world_id,
                                   WebString::FromUTF8(extension_id))) {
    DOMActivityLogger* logger = new DOMActivityLogger(extension_id);
    blink::SetDOMActivityLogger(world_id, WebString::FromUTF8(extension_id),
                                logger);
  }
}

void DOMActivityLogger::LogGetter(const WebString& api_name,
                                  const WebURL& url,
                                  const WebString& title) {
  GetRendererHost()->AddDOMActionToActivityLog(
      extension_id_, api_name.Utf8(), base::Value::List(), url, title.Utf16(),
      DomActionType::GETTER);
}

void DOMActivityLogger::LogSetter(const WebString& api_name,
                                  const v8::Local<v8::Value>& new_value,
                                  const WebURL& url,
                                  const WebString& title) {
  logSetter(api_name, new_value, v8::Local<v8::Value>(), url, title);
}

void DOMActivityLogger::logSetter(const WebString& api_name,
                                  const v8::Local<v8::Value>& new_value,
                                  const v8::Local<v8::Value>& old_value,
                                  const WebURL& url,
                                  const WebString& title) {
  base::Value::List args;
  std::string api_name_utf8 = api_name.Utf8();
  AppendV8Value(api_name_utf8, new_value, args);
  if (!old_value.IsEmpty())
    AppendV8Value(api_name_utf8, old_value, args);
  GetRendererHost()->AddDOMActionToActivityLog(
      extension_id_, api_name_utf8, std::move(args), url, title.Utf16(),
      DomActionType::SETTER);
}

void DOMActivityLogger::LogMethod(const WebString& api_name,
                                  int argc,
                                  const v8::Local<v8::Value>* argv,
                                  const WebURL& url,
                                  const WebString& title) {
  base::Value::List args;
  std::string api_name_utf8 = api_name.Utf8();
  for (int i = 0; i < argc; ++i)
    AppendV8Value(api_name_utf8, argv[i], args);
  GetRendererHost()->AddDOMActionToActivityLog(
      extension_id_, api_name_utf8, std::move(args), url, title.Utf16(),
      DomActionType::METHOD);
}

void DOMActivityLogger::LogEvent(const WebString& event_name,
                                 int argc,
                                 const WebString* argv,
                                 const WebURL& url,
                                 const WebString& title) {
  base::Value::List args;
  std::string event_name_utf8 = event_name.Utf8();
  for (int i = 0; i < argc; ++i)
    args.Append(argv[i].Utf8());
  GetRendererHost()->AddDOMActionToActivityLog(
      extension_id_, event_name_utf8, std::move(args), url, title.Utf16(),
      DomActionType::METHOD);
}

mojom::RendererHost* DOMActivityLogger::GetRendererHost() {
  if (!renderer_host_.is_bound()) {
    content::RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
        &renderer_host_);
  }
  return renderer_host_.get();
}

}  // namespace extensions

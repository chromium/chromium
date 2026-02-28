// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dom_activity_logger.h"

#include <memory>
#include <utility>

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_id.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extensions_renderer_client.h"
#include "extensions/renderer/policy_activity_log_filter.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "v8/include/v8-isolate.h"

using blink::WebString;
using blink::WebURL;

namespace extensions {

namespace {

// Converts the given |v8_value| and appends it to the given |list|, if the
// conversion succeeds.
void AppendV8Value(v8::Isolate* isolate,
                   const std::string& api_name,
                   const v8::Local<v8::Value>& v8_value,
                   base::ListValue& list) {
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  ActivityLogConverterStrategy strategy;
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);
  std::unique_ptr<base::Value> value(
      converter->FromV8Value(v8_value, isolate->GetCurrentContext()));

  if (value)
    list.Append(base::Value::FromUniquePtrValue(std::move(value)));
}

}  // namespace

DOMActivityLogger::DOMActivityLogger(const ExtensionId& extension_id)
    : extension_id_(extension_id) {
  CHECK(!extension_id_.empty());
}

DOMActivityLogger::~DOMActivityLogger() = default;

void DOMActivityLogger::AttachToWorldIfEnabled(
    int32_t world_id,
    const ExtensionId& extension_id) {
  ExtensionsRendererClient* client = ExtensionsRendererClient::Get();
  if (client->IsActivityLoggingEnabled() ||
      client->IsPolicyActivityLoggingEnabled()) {
    AttachToWorld(world_id, extension_id);
  }
}

void DOMActivityLogger::LogGetter(v8::Isolate* isolate,
                                  v8::Local<v8::Context> context,
                                  const WebString& api_name,
                                  const WebURL& url,
                                  const WebString& title) {
  auto* renderer_host = GetRendererHost(context);
  if (!renderer_host) {
    return;
  }

  LogInternal(renderer_host, DomActionType::GETTER, api_name.Utf8(),
              base::ListValue(), url, title.Utf16());
}

void DOMActivityLogger::LogSetter(v8::Isolate* isolate,
                                  v8::Local<v8::Context> context,
                                  const WebString& api_name,
                                  const v8::Local<v8::Value>& new_value,
                                  const WebURL& url,
                                  const WebString& title) {
  auto* renderer_host = GetRendererHost(context);
  if (!renderer_host) {
    return;
  }

  base::ListValue args;
  std::string api_name_utf8 = api_name.Utf8();
  AppendV8Value(isolate, api_name_utf8, new_value, args);

  LogInternal(renderer_host, DomActionType::SETTER, api_name_utf8,
              std::move(args), url, title.Utf16());
}

void DOMActivityLogger::LogMethod(v8::Isolate* isolate,
                                  v8::Local<v8::Context> context,
                                  const WebString& api_name,
                                  base::span<const v8::Local<v8::Value>> argv,
                                  const WebURL& url,
                                  const WebString& title) {
  auto* renderer_host = GetRendererHost(context);
  if (!renderer_host) {
    return;
  }

  base::ListValue args;
  std::string api_name_utf8 = api_name.Utf8();
  for (const auto& arg : argv) {
    AppendV8Value(isolate, api_name_utf8, arg, args);
  }

  LogInternal(renderer_host, DomActionType::METHOD, api_name_utf8,
              std::move(args), url, title.Utf16());
}

void DOMActivityLogger::LogEvent(blink::WebLocalFrame& frame,
                                 const WebString& event_name,
                                 base::span<const WebString> argv,
                                 const WebURL& url,
                                 const WebString& title) {
  auto* renderer_host =
      ExtensionFrameHelper::Get(content::RenderFrame::FromWebFrame(&frame))
          ->GetRendererHost();
  if (!renderer_host) {
    return;
  }

  base::ListValue args;
  for (const auto& arg : argv) {
    args.Append(arg.Utf8());
  }

  LogInternal(renderer_host, DomActionType::METHOD, event_name.Utf8(),
              std::move(args), url, title.Utf16());
}

void DOMActivityLogger::AttachToWorld(int32_t world_id,
                                      const ExtensionId& extension_id) {
  // If there is no logger registered for world_id, construct a new logger
  // and register it with world_id.
  if (!blink::HasDOMActivityLogger(world_id,
                                   WebString::FromUTF8(extension_id))) {
    DOMActivityLogger* logger = new DOMActivityLogger(extension_id);
    blink::SetDOMActivityLogger(world_id, WebString::FromUTF8(extension_id),
                                logger);
  }
}

void DOMActivityLogger::LogInternal(mojom::RendererHost* renderer_host,
                                    DomActionType::Type type,
                                    const std::string& api_name,
                                    base::ListValue args,
                                    const GURL& url,
                                    const std::u16string& title) {
  CHECK(renderer_host);

  ExtensionsRendererClient* client = ExtensionsRendererClient::Get();
  bool should_log = client->IsActivityLoggingEnabled();

  if (!should_log && client->IsPolicyActivityLoggingEnabled()) {
    PolicyActivityLogFilter* filter = client->GetPolicyActivityLogFilter();
    should_log = filter && filter->IsHighRiskEvent(extension_id_, type,
                                                   api_name, args, url);
  }

  if (should_log) {
    renderer_host->AddDOMActionToActivityLog(extension_id_, api_name,
                                             std::move(args), url, title,
                                             static_cast<int32_t>(type));
  }
}

mojom::RendererHost* DOMActivityLogger::GetRendererHost(
    v8::Local<v8::Context> context) {
  ScriptContext* script_context =
      ScriptContextSet::GetContextByV8Context(context);
  if (!script_context) {
    return nullptr;
  }
  auto* frame = script_context->GetRenderFrame();
  if (!frame) {
    return nullptr;
  }
  return ExtensionFrameHelper::Get(frame)->GetRendererHost();
}

}  // namespace extensions

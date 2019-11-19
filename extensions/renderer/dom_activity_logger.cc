// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/dom_activity_logger.h"

#include <utility>

#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/dom_action_types.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/activity_log_converter_strategy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"

using blink::WebString;
using blink::WebURL;

namespace extensions {

namespace {

// Converts the given |v8_value| and appends it to the given |list|, if the
// conversion succeeds.
void AppendV8Value(const std::string& api_name,
                   const v8::Local<v8::Value>& v8_value,
                   base::ListValue* list) {
  DCHECK(list);
  std::unique_ptr<content::V8ValueConverter> converter =
      content::V8ValueConverter::Create();
  ActivityLogConverterStrategy strategy;
  converter->SetFunctionAllowed(true);
  converter->SetStrategy(&strategy);
  std::unique_ptr<base::Value> value(converter->FromV8Value(
      v8_value, v8::Isolate::GetCurrent()->GetCurrentContext()));

  if (value.get())
    list->Append(std::move(value));
}

}  // namespace

DOMActivityLogger::DOMActivityLogger(const std::string& extension_id)
    : extension_id_(extension_id) {
}

DOMActivityLogger::~DOMActivityLogger() {}

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
  SendDomActionMessage(api_name.Utf8(), url, title.Utf16(),
                       DomActionType::GETTER,
                       std::unique_ptr<base::ListValue>(new base::ListValue()));
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
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  std::string api_name_utf8 = api_name.Utf8();
  AppendV8Value(api_name_utf8, new_value, args.get());
  if (!old_value.IsEmpty())
    AppendV8Value(api_name_utf8, old_value, args.get());
  SendDomActionMessage(api_name_utf8, url, title.Utf16(), DomActionType::SETTER,
                       std::move(args));
}

void DOMActivityLogger::LogMethod(const WebString& api_name,
                                  int argc,
                                  const v8::Local<v8::Value>* argv,
                                  const WebURL& url,
                                  const WebString& title) {
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  std::string api_name_utf8 = api_name.Utf8();
  for (int i = 0; i < argc; ++i)
    AppendV8Value(api_name_utf8, argv[i], args.get());
  SendDomActionMessage(api_name_utf8, url, title.Utf16(), DomActionType::METHOD,
                       std::move(args));
}

void DOMActivityLogger::LogEvent(const WebString& event_name,
                                 int argc,
                                 const WebString* argv,
                                 const WebURL& url,
                                 const WebString& title) {
  std::unique_ptr<base::ListValue> args(new base::ListValue);
  std::string event_name_utf8 = event_name.Utf8();
  for (int i = 0; i < argc; ++i)
    args->AppendString(argv[i].Utf8());
  SendDomActionMessage(event_name_utf8, url, title.Utf16(),
                       DomActionType::METHOD, std::move(args));
}

void DOMActivityLogger::SendDomActionMessage(
    const std::string& api_call,
    const GURL& url,
    const base::string16& url_title,
    DomActionType::Type call_type,
    std::unique_ptr<base::ListValue> args) {
  ExtensionHostMsg_DOMAction_Params params;
  params.api_call = api_call;
  params.url = url;
  params.url_title = url_title;
  params.call_type = call_type;
  params.arguments.Swap(args.get());
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_AddDOMActionToActivityLog(extension_id_, params));
}

}  // namespace extensions

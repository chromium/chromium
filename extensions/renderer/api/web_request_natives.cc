// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/web_request_natives.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/api/web_request/web_request_filter.h"
#include "extensions/common/api/web_request/web_request_resource_type.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/script_context.h"
#include "url/gurl.h"
#include "v8/include/v8-container.h"

namespace extensions {

namespace {

// Parses a webRequest.RequestFilter into a WebRequestParsedFilter, or returns
// nullopt if the browser would also reject it.
std::optional<WebRequestParsedFilter> ParseListenerFilter(
    v8::Local<v8::Value> filter_value,
    v8::Local<v8::Context> v8_context) {
  std::unique_ptr<base::Value> parsed_value =
      content::V8ValueConverter::Create()->FromV8Value(filter_value,
                                                       v8_context);
  const base::DictValue* dict =
      parsed_value ? parsed_value->GetIfDict() : nullptr;
  if (!dict) {
    return std::nullopt;
  }
  WebRequestParsedFilter parsed;
  // The parse error is unused here: the browser rejects the listener's
  // registration with the same error, surfacing it to the developer there.
  // TODO(crbug.com/494684626): consider reporting the error synchronously at
  // addListener time instead, once the legacy registration path is gone.
  std::string error;
  if (!parsed.InitFromValue(*dict, &error)) {
    return std::nullopt;
  }
  return parsed;
}

}  // namespace

WebRequestNatives::WebRequestNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

WebRequestNatives::~WebRequestNatives() = default;

void WebRequestNatives::AddRoutes() {
  RouteHandlerFunction(
      "AllowAsyncResponsesForAllEvents",
      base::BindRepeating(&WebRequestNatives::AllowAsyncResponsesForAllEvents,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "IsPerContextEventDispatchEnabled",
      base::BindRepeating(&WebRequestNatives::IsPerContextEventDispatchEnabled,
                          base::Unretained(this)));
  RouteHandlerFunction("TrackListener",
                       base::BindRepeating(&WebRequestNatives::TrackListener,
                                           base::Unretained(this)));
  RouteHandlerFunction("UntrackListener",
                       base::BindRepeating(&WebRequestNatives::UntrackListener,
                                           base::Unretained(this)));
  RouteHandlerFunction(
      "GetMatchingListeners",
      base::BindRepeating(&WebRequestNatives::GetMatchingListeners,
                          base::Unretained(this)));
}

void WebRequestNatives::AllowAsyncResponsesForAllEvents(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(0, args.Length());

  const Extension* extension = context()->extension();
  bool always_allowed_async_handlers =
      extension && extension->manifest_version() >= 3 &&
      Manifest::IsPolicyLocation(extension->location());

  args.GetReturnValue().Set(always_allowed_async_handlers);
}

void WebRequestNatives::IsPerContextEventDispatchEnabled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(0, args.Length());
  args.GetReturnValue().Set(base::FeatureList::IsEnabled(
      extensions_features::kWebRequestPerContextEventDispatch));
}

void WebRequestNatives::TrackListener(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(6, args.Length());
  CHECK(args[0]->IsString());   // eventName
  CHECK(args[1]->IsInt32());    // id
  CHECK(args[2]->IsObject());   // filter
  CHECK(args[3]->IsInt32());    // webViewInstanceId
  CHECK(args[4]->IsBoolean());  // isBlocking
  CHECK(args[5]->IsBoolean());  // isAsyncBlocking

  v8::Isolate* isolate = args.GetIsolate();
  const std::string event_name(*v8::String::Utf8Value(isolate, args[0]));
  const int listener_id = args[1].As<v8::Int32>()->Value();

  std::optional<WebRequestParsedFilter> filter =
      ParseListenerFilter(args[2], context()->v8_context());
  if (!filter) {
    // Don't track a listener whose filter fails to parse: it could never
    // match, since the browser rejects its registration.
    return;
  }

  TrackedListener listener;
  listener.event_name = event_name;
  listener.filter = std::move(*filter);
  listener.web_view_instance_id = args[3].As<v8::Int32>()->Value();
  listener.is_blocking = args[4].As<v8::Boolean>()->Value();
  listener.is_async_blocking = args[5].As<v8::Boolean>()->Value();

  auto [_, inserted] =
      tracked_listeners_.emplace(listener_id, std::move(listener));
  // An ID should never be reused across events or registrations.
  CHECK(inserted);
}

void WebRequestNatives::UntrackListener(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsInt32());  // id

  // NOTE: no-op for untracked IDs, e.g. listener whose filter failed to parse.
  tracked_listeners_.erase(args[0].As<v8::Int32>()->Value());
}

void WebRequestNatives::GetMatchingListeners(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(7, args.Length());
  CHECK(args[0]->IsString());   // eventName
  CHECK(args[1]->IsString());   // url
  CHECK(args[2]->IsString());   // type
  CHECK(args[3]->IsInt32());    // tabId
  CHECK(args[4]->IsInt32());    // windowId
  CHECK(args[5]->IsInt32());    // instanceId
  CHECK(args[6]->IsBoolean());  // wantsResponse

  v8::Isolate* isolate = args.GetIsolate();

  const std::string event_name(*v8::String::Utf8Value(isolate, args[0]));
  const GURL url(*v8::String::Utf8Value(isolate, args[1]));
  const std::string type(*v8::String::Utf8Value(isolate, args[2]));
  const int tab_id = args[3].As<v8::Int32>()->Value();
  const int window_id = args[4].As<v8::Int32>()->Value();
  const int instance_id = args[5].As<v8::Int32>()->Value();
  const bool wants_response = args[6].As<v8::Boolean>()->Value();

  // Parse the request's resource type to match against filter types.
  WebRequestResourceType request_type = WebRequestResourceType::OTHER;
  if (!ParseWebRequestResourceType(type, &request_type)) {
    args.GetReturnValue().Set(v8::Array::New(isolate));
    return;
  }

  // Iterate over tracked listeners, considering only `event_name`'s.
  v8::LocalVector<v8::Value> matches(isolate);
  for (const auto& [listener_id, listener] : tracked_listeners_) {
    if (listener.event_name != event_name) {
      continue;
    }
    if (listener.web_view_instance_id != instance_id) {
      // NOTE: `web_view_instance_id` == 0 for non-webviews.
      continue;
    }
    // The browser excludes blocking listeners from dispatches that do not
    // want a response (e.g. an extension's synchronous XHR must not block on
    // its own listeners).
    if (!wants_response &&
        (listener.is_blocking || listener.is_async_blocking)) {
      continue;
    }
    const WebRequestParsedFilter& filter = listener.filter;
    // NOTE: keep the following filter matching logic in sync with the
    // browser's `WebRequestEventRouter::ListenerMatchesRequest()`.
    if (!filter.urls.is_empty() && !filter.urls.MatchesURL(url)) {
      continue;
    }
    if (filter.tab_id != -1 && tab_id != filter.tab_id) {
      continue;
    }
    if (filter.window_id != -1 && window_id != filter.window_id) {
      continue;
    }
    if (!filter.types.empty() &&
        !std::ranges::contains(filter.types, request_type)) {
      continue;
    }
    matches.push_back(v8::Integer::New(isolate, listener_id));
  }

  // Return the matching listeners IDs.
  args.GetReturnValue().Set(
      v8::Array::New(isolate, matches.data(), matches.size()));
}

}  // namespace extensions

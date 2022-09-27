// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage_trace_utils.h"

#include <sstream>

#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-blink.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

namespace {

template <typename T>
std::string MojoEnumToString(T value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

}  // namespace

std::unique_ptr<TracedValue> CacheStorageTracedValue(const String& string) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("string", string);
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const mojom::blink::FetchAPIRequestPtr& request) {
  auto value = std::make_unique<TracedValue>();
  if (request) {
    value->SetString("url", request->url.GetString());
    value->SetString("method",
                     String(MojoEnumToString(request->method).data()));
    value->SetString("mode", String(MojoEnumToString(request->mode).data()));
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const WTF::Vector<mojom::blink::FetchAPIRequestPtr>& requests) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("count", requests.size());
  if (!requests.empty()) {
    value->SetValue("first", CacheStorageTracedValue(requests.front()).get());
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const mojom::blink::CacheQueryOptionsPtr& options) {
  auto value = std::make_unique<TracedValue>();
  if (options) {
    value->SetBoolean("ignore_method", options->ignore_method);
    value->SetBoolean("ignore_search", options->ignore_search);
    value->SetBoolean("ignore_vary", options->ignore_vary);
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const mojom::blink::MultiCacheQueryOptionsPtr& options) {
  if (!options)
    return std::make_unique<TracedValue>();
  std::unique_ptr<TracedValue> value =
      CacheStorageTracedValue(options->query_options);
  if (!options->cache_name.IsNull()) {
    value->SetString("cache_name", options->cache_name);
  }
  return value;
}

std::string CacheStorageTracedValue(mojom::blink::CacheStorageError error) {
  return MojoEnumToString(error);
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const mojom::blink::FetchAPIResponsePtr& response) {
  auto value = std::make_unique<TracedValue>();
  if (response) {
    if (!response->url_list.empty()) {
      value->SetString("url", response->url_list.back().GetString());
    }
    value->SetString("type",
                     String(MojoEnumToString(response->response_type).data()));
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const WTF::Vector<mojom::blink::FetchAPIResponsePtr>& responses) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("count", responses.size());
  if (!responses.empty()) {
    value->SetValue("first", CacheStorageTracedValue(responses.front()).get());
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const mojom::blink::BatchOperationPtr& op) {
  auto value = std::make_unique<TracedValue>();
  if (op) {
    value->SetValue("request", CacheStorageTracedValue(op->request).get());
    value->SetValue("response", CacheStorageTracedValue(op->response).get());
    value->SetValue("options",
                    CacheStorageTracedValue(op->match_options).get());
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const WTF::Vector<String>& string_list) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("count", string_list.size());
  if (!string_list.empty()) {
    value->SetString("first", string_list.front());
  }
  return value;
}

}  // namespace blink

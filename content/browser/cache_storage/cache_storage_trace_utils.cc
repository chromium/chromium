// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_trace_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/traced_value.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"

namespace content {

namespace {

template <typename T>
std::string MojoEnumToString(T value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

}  // namespace

using base::trace_event::TracedValue;

std::string CacheStorageTracedValue(blink::mojom::CacheStorageError error) {
  return MojoEnumToString(error);
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const blink::mojom::FetchAPIRequestPtr& request) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  if (request) {
    value->SetString("url", request->url.spec());
    value->SetString("method", MojoEnumToString(request->method));
    value->SetString("mode", MojoEnumToString(request->mode));
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& request_list) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  value->SetInteger("count", request_list.size());
  if (!request_list.empty()) {
    value->SetValue("first",
                    CacheStorageTracedValue(request_list.front()).get());
  }
  return value;
}

std::unique_ptr<TracedValue> CacheStorageTracedValue(
    const blink::mojom::FetchAPIResponsePtr& response) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  if (response) {
    if (!response->url_list.empty())
      value->SetString("url", response->url_list.back().spec());
    value->SetString("type", MojoEnumToString(response->response_type));
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::FetchAPIResponsePtr>& response_list) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  value->SetInteger("count", response_list.size());
  if (!response_list.empty()) {
    value->SetValue("first",
                    CacheStorageTracedValue(response_list.front()).get());
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::CacheQueryOptionsPtr& options) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  if (options) {
    value->SetBoolean("ignore_method", options->ignore_method);
    value->SetBoolean("ignore_search", options->ignore_search);
    value->SetBoolean("ignore_vary", options->ignore_vary);
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::MultiCacheQueryOptionsPtr& options) {
  if (!options)
    return std::make_unique<TracedValue>();
  std::unique_ptr<TracedValue> value =
      CacheStorageTracedValue(options->query_options);
  if (options->cache_name) {
    value->SetString("cache_name", base::UTF16ToUTF8(*options->cache_name));
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::BatchOperationPtr& op) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  if (op) {
    value->SetString("operation_type", MojoEnumToString(op->operation_type));
    value->SetValue("request", CacheStorageTracedValue(op->request).get());
    value->SetValue("response", CacheStorageTracedValue(op->response).get());
    value->SetValue("options",
                    CacheStorageTracedValue(op->match_options).get());
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::BatchOperationPtr>& operation_list) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  value->SetInteger("count", operation_list.size());
  if (!operation_list.empty()) {
    value->SetValue("first",
                    CacheStorageTracedValue(operation_list.front()).get());
  }
  return value;
}

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<base::string16> string_list) {
  std::unique_ptr<TracedValue> value = std::make_unique<TracedValue>();
  value->SetInteger("count", string_list.size());
  if (!string_list.empty()) {
    value->SetString("first", base::UTF16ToUTF8(string_list.front()));
  }
  return value;
}

}  // namespace content

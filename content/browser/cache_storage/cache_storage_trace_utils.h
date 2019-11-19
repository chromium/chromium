// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_TRACE_UTILS_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_TRACE_UTILS_H_

#include <memory>
#include <vector>

#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-forward.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom-forward.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace content {

// The following are a set of helper functions to convert a cache_storage
// related value into something that can be passed to the TRACE_EVENT*
// macros.
//
// Note, these are designed to use content mojo types and base::TracedValue.
// Unforfortunately these types are not usable in blink, so these routines
// must be duplicated there as well.

std::string CacheStorageTracedValue(blink::mojom::CacheStorageError error);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::FetchAPIRequestPtr& request);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::FetchAPIRequestPtr>& request_list);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::FetchAPIResponsePtr& response);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::FetchAPIResponsePtr>& response_list);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::CacheQueryOptionsPtr& options);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::MultiCacheQueryOptionsPtr& options);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const blink::mojom::BatchOperationPtr& op);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<blink::mojom::BatchOperationPtr>& operation_list);

std::unique_ptr<base::trace_event::TracedValue> CacheStorageTracedValue(
    const std::vector<base::string16> string_list);

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_TRACE_UTILS_H_

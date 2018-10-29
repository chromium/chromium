// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/testing/worker_internals_fetch.h"

#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Vector<String> WorkerInternalsFetch::getInternalResponseURLList(
    WorkerInternals& internals,
    Response* response) {
  if (!response)
    return Vector<String>();
  Vector<String> url_list;
  url_list.ReserveCapacity(response->InternalURLList().size());
  for (const auto& url : response->InternalURLList())
    url_list.push_back(url);
  return url_list;
}

int WorkerInternalsFetch::getResourcePriority(
    WorkerInternals& internals,
    const String& url,
    WorkerGlobalScope* worker_global) {
  if (!worker_global)
    return static_cast<int>(ResourceLoadPriority::kUnresolved);

  Resource* resource = worker_global->Fetcher()->AllResources().at(
      url_test_helpers::ToKURL(url.Utf8().data()));

  if (!resource)
    return static_cast<int>(ResourceLoadPriority::kUnresolved);

  return static_cast<int>(resource->GetResourceRequest().Priority());
}

}  // namespace blink

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/worker_resource_fetcher_properties.h"

#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

WorkerResourceFetcherProperties::WorkerResourceFetcherProperties(
    WorkerOrWorkletGlobalScope& global_scope,
    const FetchClientSettingsObject& fetch_client_settings_object,
    scoped_refptr<WebWorkerFetchContext> web_context)
    : global_scope_(global_scope),
      fetch_client_settings_object_(fetch_client_settings_object),
      web_context_(std::move(web_context)),
      outstanding_throttled_limit_(
          global_scope_->GetOutstandingThrottledLimit()) {
  DCHECK(web_context_);
}

void WorkerResourceFetcherProperties::Trace(Visitor* visitor) const {
  visitor->Trace(global_scope_);
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

mojom::ControllerServiceWorkerMode
WorkerResourceFetcherProperties::GetControllerServiceWorkerMode() const {
  return web_context_->GetControllerServiceWorkerMode();
}

bool WorkerResourceFetcherProperties::IsPaused() const {
  return global_scope_->IsContextPaused();
}

LoaderFreezeMode WorkerResourceFetcherProperties::FreezeMode() const {
  return global_scope_->GetLoaderFreezeMode();
}

int WorkerResourceFetcherProperties::GetOutstandingThrottledLimit() const {
  return outstanding_throttled_limit_;
}

}  // namespace blink

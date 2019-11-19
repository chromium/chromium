// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/frame_resource_fetcher_properties.h"

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_or_imported_document.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/fetch_client_settings_object_impl.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

namespace blink {

FrameResourceFetcherProperties::FrameResourceFetcherProperties(
    FrameOrImportedDocument& frame_or_imported_document)
    : frame_or_imported_document_(frame_or_imported_document),
      fetch_client_settings_object_(
          MakeGarbageCollected<FetchClientSettingsObjectImpl>(
              frame_or_imported_document.GetDocument())) {}

void FrameResourceFetcherProperties::Trace(Visitor* visitor) {
  visitor->Trace(frame_or_imported_document_);
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

bool FrameResourceFetcherProperties::IsMainFrame() const {
  return frame_or_imported_document_->GetFrame().IsMainFrame();
}

mojom::ControllerServiceWorkerMode
FrameResourceFetcherProperties::GetControllerServiceWorkerMode() const {
  auto* service_worker_network_provider =
      frame_or_imported_document_->GetMasterDocumentLoader()
          .GetServiceWorkerNetworkProvider();
  if (!service_worker_network_provider)
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return service_worker_network_provider->GetControllerServiceWorkerMode();
}

int64_t FrameResourceFetcherProperties::ServiceWorkerId() const {
  DCHECK_NE(GetControllerServiceWorkerMode(),
            blink::mojom::ControllerServiceWorkerMode::kNoController);
  auto* service_worker_network_provider =
      frame_or_imported_document_->GetMasterDocumentLoader()
          .GetServiceWorkerNetworkProvider();
  DCHECK(service_worker_network_provider);
  return service_worker_network_provider->ControllerServiceWorkerID();
}

bool FrameResourceFetcherProperties::IsPaused() const {
  return frame_or_imported_document_->GetFrame().GetPage()->Paused();
}

bool FrameResourceFetcherProperties::IsLoadComplete() const {
  return frame_or_imported_document_->GetDocument().LoadEventFinished();
}

bool FrameResourceFetcherProperties::ShouldBlockLoadingSubResource() const {
  DocumentLoader* document_loader =
      frame_or_imported_document_->GetDocumentLoader();
  if (!document_loader)
    return false;

  FrameLoader& loader = frame_or_imported_document_->GetFrame().Loader();
  return document_loader != loader.GetDocumentLoader();
}

bool FrameResourceFetcherProperties::IsSubframeDeprioritizationEnabled() const {
  Settings* settings = frame_or_imported_document_->GetFrame().GetSettings();
  if (!settings) {
    return false;
  }

  const WebEffectiveConnectionType max_effective_connection_type_threshold =
      settings->GetLowPriorityIframesThreshold();
  if (max_effective_connection_type_threshold <=
      WebEffectiveConnectionType::kTypeOffline) {
    return false;
  }

  const WebEffectiveConnectionType effective_connection_type =
      GetNetworkStateNotifier().EffectiveType();
  if (effective_connection_type <= WebEffectiveConnectionType::kTypeOffline) {
    return false;
  }

  if (effective_connection_type > max_effective_connection_type_threshold) {
    // Network is not slow enough.
    return false;
  }

  return true;
}

scheduler::FrameStatus FrameResourceFetcherProperties::GetFrameStatus() const {
  return scheduler::GetFrameStatus(
      frame_or_imported_document_->GetFrame().GetFrameScheduler());
}

}  // namespace blink

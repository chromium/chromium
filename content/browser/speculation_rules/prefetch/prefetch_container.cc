// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_container.h"

#include <memory>

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"
#include "content/browser/speculation_rules/prefetch/prefetch_network_context.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/browser/speculation_rules/prefetch/prefetched_mainframe_response_container.h"
#include "content/public/browser/global_routing_id.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace content {

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      url_(url),
      prefetch_type_(prefetch_type),
      prefetch_document_manager_(prefetch_document_manager) {}

PrefetchContainer::~PrefetchContainer() = default;

PrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

PrefetchNetworkContext* PrefetchContainer::GetOrCreateNetworkContext(
    PrefetchService* prefetch_service) {
  if (!network_context_) {
    network_context_ = std::make_unique<PrefetchNetworkContext>(
        prefetch_service, prefetch_type_);
  }
  return network_context_.get();
}

PrefetchDocumentManager* PrefetchContainer::GetPrefetchDocumentManager() const {
  return prefetch_document_manager_.get();
}

void PrefetchContainer::TakeURLLoader(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  DCHECK(!loader_);
  loader_ = std::move(loader);
}

void PrefetchContainer::ResetURLLoader() {
  DCHECK(loader_);
  loader_.reset();
}

bool PrefetchContainer::HasPrefetchedResponse() const {
  return prefetched_response_ != nullptr;
}

void PrefetchContainer::TakePrefetchedResponse(
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response) {
  prefetched_response_ = std::move(prefetched_response);
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ReleasePrefetchedResponse() {
  return std::move(prefetched_response_);
}

}  // namespace content

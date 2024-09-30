// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_for_shared_worker.h"

#include "base/ranges/algorithm.h"
#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/browser_thread.h"

namespace content {

PressureServiceForSharedWorker::PressureServiceForSharedWorker(
    SharedWorkerHost* host)
    : worker_host_(host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PressureServiceForSharedWorker::~PressureServiceForSharedWorker() = default;

bool PressureServiceForSharedWorker::ShouldDeliverUpdate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // https://www.w3.org/TR/compute-pressure/#dfn-owning-document-set
  // https://www.w3.org/TR/compute-pressure/#dfn-may-receive-data
  if (base::ranges::any_of(
          worker_host_->GetRenderFrameIDsForWorker(), [](const auto& id) {
            return HasImplicitFocus(RenderFrameHostImpl::FromID(id));
          })) {
    return true;
  }
  return false;
}

std::optional<base::UnguessableToken>
PressureServiceForSharedWorker::GetTokenFor(
    device::mojom::PressureSource source) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Shared workers always return std::nullopt, as there is no one
  // corresponding WebContents instance to retrieve a
  // WebContentsPressureManagerProxy instance from.
  return std::nullopt;
}

}  // namespace content

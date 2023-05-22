// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/environment_integrity/environment_integrity_service_impl.h"

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

EnvironmentIntegrityServiceImpl::EnvironmentIntegrityServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

EnvironmentIntegrityServiceImpl::~EnvironmentIntegrityServiceImpl() = default;

// static
void EnvironmentIntegrityServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver) {
  CHECK(render_frame_host);

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new EnvironmentIntegrityServiceImpl(*render_frame_host, std::move(receiver));
}

void EnvironmentIntegrityServiceImpl::GetEnvironmentIntegrity(
    GetEnvironmentIntegrityCallback callback) {
  // TODO(crbug.com/1439945) Get integrity token from PIA Crystal API.
  std::move(callback).Run();
}

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_SERVICE_IMPL_H_
#define CONTENT_BROWSER_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_SERVICE_IMPL_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.h"

namespace content {

class EnvironmentIntegrityServiceImpl final
    : public content::DocumentService<
          blink::mojom::EnvironmentIntegrityService> {
 public:
  EnvironmentIntegrityServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService>
          receiver);

  EnvironmentIntegrityServiceImpl(const EnvironmentIntegrityServiceImpl&) =
      delete;
  EnvironmentIntegrityServiceImpl& operator=(
      const EnvironmentIntegrityServiceImpl&) = delete;
  ~EnvironmentIntegrityServiceImpl() override;

  // Factory method for creating an instance of this interface.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService>
          receiver);

  // blink::mojom::EnvironmentIntegrityService:
  void GetEnvironmentIntegrity(
      GetEnvironmentIntegrityCallback callback) override;

  base::WeakPtrFactory<EnvironmentIntegrityServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ENVIRONMENT_INTEGRITY_ENVIRONMENT_INTEGRITY_SERVICE_IMPL_H_

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HAPTICS_HAPTICS_SERVICE_IMPL_H_
#define CONTENT_BROWSER_HAPTICS_HAPTICS_SERVICE_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/haptics/haptics.mojom.h"

namespace content {

class HapticsManager;
class RenderFrameHost;

// Browser-side implementation for the Web Haptics API.
// Serves as the authoritative security boundary that enforces checks
// for API calls.
//
// It is a DocumentService, so its lifetime is bound to the document and the
// Mojo pipe. On Windows the platform backend runs in the browser process and is
// reached with a direct in-process call; there is no browser<->device-service
// pipe.
class CONTENT_EXPORT HapticsServiceImpl final
    : public DocumentService<blink::mojom::HapticsService> {
 public:
  // Creates and binds a HapticsManager backend for a HapticsServiceImpl. Tests
  // set this to inject a fake in place of the real platform backend; a null
  // callback restores the default.
  using HapticsManagerFactory =
      base::RepeatingCallback<std::unique_ptr<HapticsManager>()>;

  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::HapticsService> receiver);

  static void SetHapticsManagerFactoryForTesting(HapticsManagerFactory factory);

  HapticsServiceImpl(const HapticsServiceImpl&) = delete;
  HapticsServiceImpl& operator=(const HapticsServiceImpl&) = delete;

  ~HapticsServiceImpl() override;

  // blink::mojom::HapticsService:
  void PlayHaptics(blink::mojom::HapticEffect effect, double intensity) final;

 private:
  HapticsServiceImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::HapticsService> receiver);

  std::unique_ptr<HapticsManager> haptics_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_HAPTICS_HAPTICS_SERVICE_IMPL_H_

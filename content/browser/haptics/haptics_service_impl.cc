// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/haptics/haptics_service_impl.h"

#include <cmath>
#include <utility>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/haptics/haptics_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "content/browser/haptics/haptics_manager_impl_win.h"
#endif

namespace content {

namespace {

HapticsServiceImpl::HapticsManagerFactory& GetTestFactory() {
  static base::NoDestructor<HapticsServiceImpl::HapticsManagerFactory> factory;
  return *factory;
}

std::unique_ptr<HapticsManager> CreateHapticsManager() {
  if (GetTestFactory()) {
    return GetTestFactory().Run();
  }
#if BUILDFLAG(IS_WIN)
  return std::make_unique<HapticsManagerImplWin>();
#else
  return nullptr;
#endif
}

}  // namespace

// static
void HapticsServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::HapticsService> receiver) {
  CHECK(render_frame_host);

  // Fenced-frame state and the "haptics" permissions policy are fixed for the
  // document's lifetime, so enforce them once at bind time.
  if (render_frame_host->IsNestedWithinFencedFrame()) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::HSI_PLAY_HAPTICS_IN_FENCED_FRAME);
    return;
  }

  if (!render_frame_host->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kHaptics)) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::HSI_PLAY_HAPTICS_BLOCKED_BY_PERMISSIONS_POLICY);
    return;
  }

  new HapticsServiceImpl(*render_frame_host, std::move(receiver));
}

// static
void HapticsServiceImpl::SetHapticsManagerFactoryForTesting(
    HapticsManagerFactory factory) {
  GetTestFactory() = std::move(factory);
}

HapticsServiceImpl::HapticsServiceImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::HapticsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      haptics_manager_(CreateHapticsManager()) {}

HapticsServiceImpl::~HapticsServiceImpl() = default;

void HapticsServiceImpl::PlayHaptics(blink::mojom::HapticEffect effect,
                                     double intensity) {
  auto& rfh = static_cast<RenderFrameHostImpl&>(render_frame_host());

  if (!std::isfinite(intensity) || intensity < 0.0 || intensity > 1.0) {
    bad_message::ReceivedBadMessage(
        rfh.GetProcess(), bad_message::HSI_PLAY_HAPTICS_INVALID_INTENSITY);
    return;
  }

  if (!rfh.IsActive()) {
    return;
  }

  if (!rfh.HasStickyUserActivation()) {
    return;
  }

  if (haptics_manager_) {
    haptics_manager_->PlayHaptics(effect, intensity);
  }
}

}  // namespace content

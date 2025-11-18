// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator.h"

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"
#endif

namespace content {

WEB_CONTENTS_USER_DATA_KEY_IMPL(PipScreenCaptureCoordinator);

PipScreenCaptureCoordinator*
PipScreenCaptureCoordinator::GetOrCreateForWebContents(
    WebContents* web_contents) {
  CHECK(web_contents);
  return WebContentsUserData<
      PipScreenCaptureCoordinator>::GetOrCreateForWebContents(web_contents);
}

PipScreenCaptureCoordinator*
PipScreenCaptureCoordinator::GetOrCreateForRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  return GetOrCreateForWebContents(web_contents);
}

PipScreenCaptureCoordinator::PipScreenCaptureCoordinator(
    WebContents* web_contents)
    : WebContentsUserData<PipScreenCaptureCoordinator>(*web_contents) {}

PipScreenCaptureCoordinator::~PipScreenCaptureCoordinator() = default;

void PipScreenCaptureCoordinator::OnPipShown(WebContents& pip_web_contents) {
#if BUILDFLAG(IS_MAC)
  if (auto* instance = PipScreenCaptureCoordinatorImpl::GetInstance()) {
    instance->OnPipShown(pip_web_contents);
  }
#endif
}

void PipScreenCaptureCoordinator::OnPipClosed() {
#if BUILDFLAG(IS_MAC)
  if (auto* instance = PipScreenCaptureCoordinatorImpl::GetInstance()) {
    instance->OnPipClosed();
  }
#endif
}

std::unique_ptr<PipScreenCaptureCoordinatorProxy>
PipScreenCaptureCoordinator::CreateProxy() {
#if BUILDFLAG(IS_MAC)
  if (auto* instance = PipScreenCaptureCoordinatorImpl::GetInstance()) {
    return instance->CreateProxy();
  }
#endif
  return nullptr;
}

}  // namespace content

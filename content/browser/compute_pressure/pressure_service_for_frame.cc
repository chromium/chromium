// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_for_frame.h"

#include "base/system/sys_info.h"
#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

PressureServiceForFrame::PressureServiceForFrame(
    RenderFrameHost* render_frame_host)
    : DocumentUserData<PressureServiceForFrame>(render_frame_host),
      metrics_(
#if BUILDFLAG(IS_MAC)
          base::ProcessMetrics::CreateProcessMetrics(
              render_frame_host->GetProcess()->GetProcess().Handle(),
              BrowserChildProcessHost::GetPortProvider())
#else
          base::ProcessMetrics::CreateProcessMetrics(
              render_frame_host->GetProcess()->GetProcess().Handle())
#endif  // BUILDFLAG(IS_MAC)
      ) {
  CHECK(render_frame_host);
}

PressureServiceForFrame::~PressureServiceForFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Manually remove the observer here, due to a possible race between
  // RenderFrameHost deletion and WebPressureManager's receiver
  // disconnect_handler call.
  //
  // The observer is removed here and not in PressureServiceBase because
  // DocumentUserData needs to be valid to retrieve the RenderFrameHost
  // necessary to access WebContents.
  auto* pressure_manager_proxy = GetWebContentsPressureManagerProxy();
  if (pressure_manager_proxy) {
    pressure_manager_proxy->RemoveObserver(this);
  }
}

bool PressureServiceForFrame::CanCallAddClient() const {
  return render_frame_host().IsActive() &&
         !render_frame_host().IsNestedWithinFencedFrame();
}

bool PressureServiceForFrame::ShouldDeliverUpdate() const {
  return HasImplicitFocus(&render_frame_host());
}

std::optional<base::UnguessableToken> PressureServiceForFrame::GetTokenFor(
    device::mojom::PressureSource source) const {
  const auto* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (const auto* pressure_manager_proxy =
          WebContentsPressureManagerProxy::FromWebContents(web_contents)) {
    return pressure_manager_proxy->GetTokenFor(source);
  }
  return std::nullopt;
}

RenderFrameHost* PressureServiceForFrame::GetRenderFrameHost() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return &render_frame_host();
}

double PressureServiceForFrame::CalculateOwnContributionEstimate(
    double global_cpu_utilization) {
  double process_pressure =
      metrics_->GetPlatformIndependentCPUUsage().value_or(-1.0) /
      static_cast<double>(base::SysInfo::NumberOfProcessors());

  return process_pressure / (global_cpu_utilization * 100);
}

DOCUMENT_USER_DATA_KEY_IMPL(PressureServiceForFrame);

}  // namespace content

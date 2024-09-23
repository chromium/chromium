// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
std::unique_ptr<DocumentResourceCoordinator>
DocumentResourceCoordinator::MaybeCreate(
    const BrowserInterfaceBrokerProxy& interface_broker) {
  if (!RuntimeEnabledFeatures::PerformanceManagerInstrumentationEnabled())
    return nullptr;

  return base::WrapUnique(new DocumentResourceCoordinator(interface_broker));
}

DocumentResourceCoordinator::DocumentResourceCoordinator(
    const BrowserInterfaceBrokerProxy& interface_broker) {
  interface_broker.GetInterface(service_.BindNewPipeAndPassReceiver());
  DCHECK(service_);
}

DocumentResourceCoordinator::~DocumentResourceCoordinator() = default;

void DocumentResourceCoordinator::SetNetworkAlmostIdle() {
  service_->SetNetworkAlmostIdle();
}

void DocumentResourceCoordinator::SetLifecycleState(
    performance_manager::mojom::LifecycleState state) {
  service_->SetLifecycleState(state);
}

void DocumentResourceCoordinator::SetHasNonEmptyBeforeUnload(
    bool has_nonempty_beforeunload) {
  service_->SetHasNonEmptyBeforeUnload(has_nonempty_beforeunload);
}

void DocumentResourceCoordinator::SetIsAdFrame(bool is_ad_frame) {
  service_->SetIsAdFrame(is_ad_frame);
}

void DocumentResourceCoordinator::OnNonPersistentNotificationCreated() {
  service_->OnNonPersistentNotificationCreated();
}

void DocumentResourceCoordinator::SetHadFormInteraction() {
  // Only send this signal for the first interaction as it doesn't get cleared
  // for the lifetime of the frame and it's inefficient to send this message
  // for every keystroke.
  if (!had_form_interaction_)
    service_->SetHadFormInteraction();
  had_form_interaction_ = true;
}

void DocumentResourceCoordinator::SetHadUserEdits() {
  // Only send this signal for the first interaction as it doesn't get cleared
  // for the lifetime of the frame and it's inefficient to send this message
  // for every keystroke.
  if (!had_user_edits_) {
    service_->SetHadUserEdits();
  }
  had_user_edits_ = true;
}

void DocumentResourceCoordinator::OnStartedUsingWebRTC() {
  ++num_web_rtc_usage_;
  if (num_web_rtc_usage_ == 1) {
    service_->OnStartedUsingWebRTC();
  }
}

void DocumentResourceCoordinator::OnStoppedUsingWebRTC() {
  --num_web_rtc_usage_;
  CHECK_GE(num_web_rtc_usage_, 0);
  if (num_web_rtc_usage_ == 0) {
    service_->OnStoppedUsingWebRTC();
  }
}

void DocumentResourceCoordinator::OnFirstContentfulPaint(
    base::TimeDelta time_since_navigation_start) {
  service_->OnFirstContentfulPaint(time_since_navigation_start);
}

void DocumentResourceCoordinator::OnWebMemoryMeasurementRequested(
    WebMemoryMeasurementMode mode,
    OnWebMemoryMeasurementRequestedCallback callback) {
  service_->OnWebMemoryMeasurementRequested(mode, std::move(callback));
}

}  // namespace blink

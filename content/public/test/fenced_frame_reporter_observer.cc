// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fenced_frame_reporter_observer.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace test {

FencedFrameReporterObserverForTesting::FencedFrameReporterObserverForTesting(
    FencedFrameReporter* fenced_frame_reporter,
    const FencedFrameReporter::DestinationVariant& event_variant)
    : fenced_frame_reporter_(fenced_frame_reporter),
      event_variant_(event_variant) {
  fenced_frame_reporter_->AddObserverForTesting(this);
}

FencedFrameReporterObserverForTesting::
    ~FencedFrameReporterObserverForTesting() {
  if (fenced_frame_reporter_) {
    fenced_frame_reporter_->RemoveObserverForTesting(this);
  }
}

void FencedFrameReporterObserverForTesting::OnBeaconQueued(
    const FencedFrameReporter::DestinationVariant& event_variant,
    bool is_queued) {
  if (event_variant_ == event_variant) {
    is_event_queued_.SetValue(is_queued);
  }
}

bool FencedFrameReporterObserverForTesting::IsReportingEventQueued() {
  return is_event_queued_.Take();
}

std::unique_ptr<FencedFrameReporterObserverForTesting>
InstallFencedFrameReporterObserver(
    RenderFrameHost* fenced_frame_rfh,
    const FencedFrameReporter::DestinationVariant& event_variant) {
  std::optional<content::FencedFrameProperties> fenced_frame_properties =
      static_cast<content::RenderFrameHostImpl*>(fenced_frame_rfh)
          ->frame_tree_node()
          ->GetFencedFrameProperties();
  CHECK(fenced_frame_properties.has_value());

  scoped_refptr<content::FencedFrameReporter> fenced_frame_reporter =
      fenced_frame_properties->fenced_frame_reporter();
  CHECK(fenced_frame_reporter);

  return std::make_unique<FencedFrameReporterObserverForTesting>(
      fenced_frame_reporter.get(), event_variant);
}

}  // namespace test

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FENCED_FRAME_REPORTER_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_FENCED_FRAME_REPORTER_OBSERVER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace test {
// A test-only observer that can be installed to `FencedFrameReporter`. It is
// used to observe whether a reporting beacon specified by `DestinationVariant`
// is queued to be sent later. This is important because when beacon is queued,
// any potential console error messages will be ignored.
class FencedFrameReporterObserverForTesting
    : public FencedFrameReporter::ObserverForTesting {
 public:
  explicit FencedFrameReporterObserverForTesting(
      FencedFrameReporter* fenced_frame_reporter,
      const FencedFrameReporter::DestinationVariant& event_variant);
  FencedFrameReporterObserverForTesting(
      const FencedFrameReporterObserverForTesting&) = delete;
  FencedFrameReporterObserverForTesting operator=(
      const FencedFrameReporterObserverForTesting&) = delete;
  ~FencedFrameReporterObserverForTesting() override;

  void OnBeaconQueued(
      const FencedFrameReporter::DestinationVariant& event_variant,
      bool is_queued) override;

  bool IsReportingEventQueued();

 private:
  raw_ptr<FencedFrameReporter> fenced_frame_reporter_;
  FencedFrameReporter::DestinationVariant event_variant_;
  base::test::TestFuture<bool> is_event_queued_;
};

// Create and install a beacon observer for the specific `event_variant` to the
// given fenced frame render frame host. Tests should use this function instead
// of constructing `BeaconObserverForTesting` directly. This is because this
// function contains the necessary pre-condition checks.
std::unique_ptr<FencedFrameReporterObserverForTesting>
InstallFencedFrameReporterObserver(
    RenderFrameHost* fenced_frame_rfh,
    const FencedFrameReporter::DestinationVariant& event_variant);

}  // namespace test

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FENCED_FRAME_REPORTER_OBSERVER_H_

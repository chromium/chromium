// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/remote_playback_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {
// static
void RemotePlaybackMetrics::RecordRemotePlaybackLocation(
    RemotePlaybackInitiationLocation location) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Sender.RemotePlayback.InitiationLocation",
                            location,
                            RemotePlaybackInitiationLocation::kMaxValue);
}

// static
void RemotePlaybackMetrics::RecordRemotePlaybackStartSessionResult(
    ExecutionContext* execution_context,
    bool success) {
  auto* ukm_recorder = execution_context->UkmRecorder();
  const ukm::SourceId source_id = execution_context->UkmSourceID();
  ukm::builders::Presentation_StartResult(source_id)
      .SetRemotePlayback(success)
      .Record(ukm_recorder);
}

}  // namespace blink

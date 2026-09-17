// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_video_perf_reporter.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"

namespace blink {

WebrtcVideoPerfReporter::WebrtcVideoPerfReporter(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    ContextLifecycleNotifier* notifier,
    mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
        perf_recorder)
    : task_runner_(task_runner),
      perf_recorder_(notifier),
      weak_handle_(MakeCrossThreadWeakHandle(this)) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  perf_recorder_.Bind(std::move(perf_recorder), task_runner_);
}

void WebrtcVideoPerfReporter::StoreWebrtcVideoStats(
    const StatsCollector::StatsKey& stats_key,
    const StatsCollector::VideoStats& video_stats) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebrtcVideoPerfReporter::StoreWebrtcVideoStatsOnTaskRunner,
          MakeUnwrappingCrossThreadWeakHandle(weak_handle_), stats_key,
          video_stats));
}

void WebrtcVideoPerfReporter::StoreWebrtcVideoStatsOnTaskRunner(
    const StatsCollector::StatsKey& stats_key,
    const StatsCollector::VideoStats& video_stats) {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!perf_recorder_.is_bound()) {
    return;
  }

  auto mojo_features = media::mojom::blink::WebrtcPredictionFeatures::New(
      stats_key.is_decode, stats_key.codec_profile, stats_key.pixel_size,
      stats_key.hw_accelerated);

  auto mojo_video_stats = media::mojom::blink::WebrtcVideoStats::New(
      video_stats.frame_count, video_stats.key_frame_count,
      video_stats.p99_processing_time_ms);
  perf_recorder_->UpdateRecord(std::move(mojo_features),
                               std::move(mojo_video_stats));
}

}  // namespace blink

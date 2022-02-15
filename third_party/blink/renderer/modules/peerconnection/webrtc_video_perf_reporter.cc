// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_video_perf_reporter.h"

#include "base/check.h"
#include "media/base/video_codecs.h"

namespace blink {

void WebrtcVideoPerfReporter::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
        perf_recorder) {
  task_runner_ = task_runner;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebrtcVideoPerfReporter::InitializeOnTaskRunner,
                     base::Unretained(this), std::move(perf_recorder)));
}

void WebrtcVideoPerfReporter::InitializeOnTaskRunner(
    mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
        perf_recorder) {
  perf_recorder_.Bind(std::move(perf_recorder));
}

void WebrtcVideoPerfReporter::StoreWebrtcVideoStats(
    const StatsCollectingDecoder::StatsKey& stats_key,
    const StatsCollectingDecoder::VideoStats& video_stats) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WebrtcVideoPerfReporter::StoreWebrtcVideoStatsOnTaskRunner,
          base::Unretained(this), stats_key, video_stats));
}

void WebrtcVideoPerfReporter::StoreWebrtcVideoStatsOnTaskRunner(
    const StatsCollectingDecoder::StatsKey& stats_key,
    const StatsCollectingDecoder::VideoStats& video_stats) {
  DCHECK(task_runner_);
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto mojo_features = media::mojom::blink::WebrtcPredictionFeatures::New(
      stats_key.is_decode,
      static_cast<media::mojom::blink::VideoCodecProfile>(
          stats_key.codec_profile),
      stats_key.pixel_size, stats_key.hw_accelerated);

  auto mojo_video_stats = media::mojom::blink::WebrtcVideoStats::New(
      video_stats.frame_count, video_stats.key_frame_count,
      video_stats.p99_processing_time_ms);
  perf_recorder_->UpdateRecord(std::move(mojo_features),
                               std::move(mojo_video_stats));
}

}  // namespace blink

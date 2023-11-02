// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_VIDEO_PERF_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_VIDEO_PERF_REPORTER_H_

#include "base/task/single_thread_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"

namespace blink {

// This class is used for WebRTC video performance stats data collection. Its
// sole purpose is to pass data collected in the render process to the browser
// process where the data is stored to a local database. The data is later used
// for smoothness predictions when the MediaCapabilities API receives a query
// for a particular video configuration.
class MODULES_EXPORT WebrtcVideoPerfReporter {
 public:
  WebrtcVideoPerfReporter();
  ~WebrtcVideoPerfReporter();

  void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingRemote<media::mojom::blink::WebrtcVideoPerfRecorder>
          perf_recorder);

  void StoreWebrtcVideoStats(const StatsCollector::StatsKey& stats_key,
                             const StatsCollector::VideoStats& video_stats);

  void Shutdown();

 private:
  void StoreWebrtcVideoStatsOnTaskRunner(
      const StatsCollector::StatsKey& stats_key,
      const StatsCollector::VideoStats& video_stats);

  base::WeakPtr<WebrtcVideoPerfReporter> weak_this_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  mojo::Remote<media::mojom::blink::WebrtcVideoPerfRecorder> perf_recorder_;
  base::WeakPtrFactory<WebrtcVideoPerfReporter> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_WEBRTC_VIDEO_PERF_REPORTER_H_

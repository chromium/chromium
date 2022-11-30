// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_RECORDER_H_
#define MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_RECORDER_H_

#include <stdint.h>

#include "media/base/video_codecs.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/webrtc_video_perf.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/webrtc_video_perf_history.h"

namespace media {

// This class implements the mojo interface WebrtcVideoPerfRecorder. The purpose
// of the class is to receive video perf data from the browser process and pass
// the data on to the WebrtcVideoPerfHistory save callback.
class MEDIA_MOJO_EXPORT WebrtcVideoPerfRecorder
    : public media::mojom::WebrtcVideoPerfRecorder {
 public:
  // Creates a WebrtcVideoPerfRecorder. `webrtc_video_perf_history` is required
  // to save stats to local database and report metrics. Callers must ensure
  // that `webrtc_video_perf_history` outlives this object; may be nullptr if
  // database recording is currently disabled.
  static void Create(
      media::WebrtcVideoPerfHistory* webrtc_video_perf_history,
      mojo::PendingReceiver<media::mojom::WebrtcVideoPerfRecorder> receiver);

  explicit WebrtcVideoPerfRecorder(
      WebrtcVideoPerfHistory::SaveCallback save_cb);
  ~WebrtcVideoPerfRecorder() override;
  WebrtcVideoPerfRecorder(const WebrtcVideoPerfRecorder&) = delete;
  WebrtcVideoPerfRecorder& operator=(const WebrtcVideoPerfRecorder&) = delete;

  // mojom::WebrtcVideoPerfRecorder implementation. Tracks `features` and
  // `video_stats` individually for encode and decode. Flushes the data to the
  // database by calling `save_cb_` whenever `features` is changed. An empty
  // pair of `features` and `video_stats` may be passed to the function to
  // signal that no more data is expected for the current `features` and force a
  // call to `save_cb_` with the current state.
  void UpdateRecord(media::mojom::WebrtcPredictionFeaturesPtr features,
                    media::mojom::WebrtcVideoStatsPtr video_stats) override;

 private:
  // Starts a new recording of stats, this essentially means flushing any
  // existing stats to the DB and storing the specified `features`.
  void StartNewRecord(media::mojom::WebrtcPredictionFeaturesPtr features);
  // Save most recent stats values to the database. Called during destruction
  // and upon starting a new record.
  void FinalizeRecord(int encode_or_decode_index);

  const WebrtcVideoPerfHistory::SaveCallback save_cb_;
  // `features_` tracks the last video configuration that was supplied to
  // UpdateRecord(). This is individually tracked for encode and decode stats
  // per the video configuration.
  media::mojom::WebrtcPredictionFeatures features_[2];
  media::mojom::WebrtcVideoStats video_stats_[2];
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_WEBRTC_VIDEO_PERF_RECORDER_H_

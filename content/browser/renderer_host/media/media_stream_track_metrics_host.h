// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

// Responsible for logging metrics about audio and video track
// lifetimes. These are based on messages from the renderer that are
// sent when tracks are created and destroyed. Unfortunately we can't
// reliably log the lifetime metric in the renderer because that
// process may be destroyed at any time by the fast shutdown path (see
// RenderProcessHost::FastShutdownIfPossible).
//
// There is one instance of this class per render process.
//
// If the renderer process goes away without sending messages that
// tracks were removed, this class instead infers that the tracks were
// removed.
class MediaStreamTrackMetricsHost
    : public blink::mojom::MediaStreamTrackMetricsHost {
 public:
  explicit MediaStreamTrackMetricsHost();

  ~MediaStreamTrackMetricsHost() override;
  void BindReceiver(
      mojo::PendingReceiver<blink::mojom::MediaStreamTrackMetricsHost>
          receiver);

 private:
  void AddTrack(uint64_t id, bool is_audio, bool is_remote) override;
  void RemoveTrack(uint64_t id) override;

  // Information for a track we're keeping in |tracks_|. |is_audio|
  // specifies whether it's an audio or video track, |is_remote|
  // specifies whether it's remote (received over a PeerConnection) or
  // local (sent over a PeerConnection). |timestamp| specifies when
  // the track was connected.
  struct TrackInfo {
    bool is_audio;
    bool is_remote;
    base::TimeTicks timestamp;
  };

  void ReportDuration(const TrackInfo& info);

  // Values are unique (per renderer) track IDs.
  typedef std::map<uint64_t, TrackInfo> TrackMap;
  TrackMap tracks_;

  mojo::ReceiverSet<blink::mojom::MediaStreamTrackMetricsHost> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_TRACK_METRICS_HOST_H_

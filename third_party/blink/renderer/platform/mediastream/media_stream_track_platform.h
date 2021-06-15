// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_TRACK_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_TRACK_PLATFORM_H_

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_track.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// MediaStreamTrackPlatform is a low-level object backing a
// WebMediaStreamTrack.
class PLATFORM_EXPORT MediaStreamTrackPlatform {
 public:
  enum class FacingMode { kNone, kUser, kEnvironment, kLeft, kRight };

  struct Settings {
    bool HasFrameRate() const { return frame_rate >= 0.0; }
    bool HasWidth() const { return width >= 0; }
    bool HasHeight() const { return height >= 0; }
    bool HasAspectRatio() const { return aspect_ratio >= 0.0; }
    bool HasFacingMode() const { return facing_mode != FacingMode::kNone; }
    bool HasSampleRate() const { return sample_rate >= 0; }
    bool HasSampleSize() const { return sample_size >= 0; }
    bool HasChannelCount() const { return channel_count >= 0; }
    bool HasLatency() const { return latency >= 0; }
    bool HasVideoKind() const { return !video_kind.IsNull(); }
    // The variables are read from
    // MediaStreamTrack::GetSettings only.
    double frame_rate = -1.0;
    int32_t width = -1;
    int32_t height = -1;
    double aspect_ratio = -1.0;
    String device_id;
    String group_id;
    FacingMode facing_mode = FacingMode::kNone;
    String resize_mode;
    absl::optional<bool> echo_cancellation;
    absl::optional<bool> auto_gain_control;
    absl::optional<bool> noise_supression;
    String echo_cancellation_type;
    int32_t sample_rate = -1;
    int32_t sample_size = -1;
    int32_t channel_count = -1;
    double latency = -1.0;

    // Media Capture Depth Stream Extensions.
    String video_kind;

    // Screen Capture extensions
    absl::optional<media::mojom::DisplayCaptureSurfaceType> display_surface;
    absl::optional<bool> logical_surface;
    absl::optional<media::mojom::CursorCaptureType> cursor;
  };

  struct CaptureHandle {
    bool IsEmpty() const { return origin.IsEmpty() && handle.IsEmpty(); }

    String origin;
    String handle;
  };

  explicit MediaStreamTrackPlatform(bool is_local_track);
  MediaStreamTrackPlatform(const MediaStreamTrackPlatform&) = delete;
  MediaStreamTrackPlatform& operator=(const MediaStreamTrackPlatform&) = delete;
  virtual ~MediaStreamTrackPlatform();

  static MediaStreamTrackPlatform* GetTrack(const WebMediaStreamTrack& track);

  virtual void SetEnabled(bool enabled) = 0;

  virtual void SetContentHint(
      WebMediaStreamTrack::ContentHintType content_hint) = 0;

  // If |callback| is not null, it is invoked when the track has stopped.
  virtual void StopAndNotify(base::OnceClosure callback) = 0;

  void Stop() { StopAndNotify(base::OnceClosure()); }

  // TODO(hta): Make method pure virtual when all tracks have the method.
  virtual void GetSettings(Settings& settings) {}
  virtual CaptureHandle GetCaptureHandle();

  bool is_local_track() const { return is_local_track_; }

 private:
  const bool is_local_track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_STREAM_TRACK_PLATFORM_H_

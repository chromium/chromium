// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_EMPTY_WEB_MEDIA_PLAYER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_EMPTY_WEB_MEDIA_PLAYER_H_

#include "base/callback.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace cc {
class PaintCanvas;
class PaintFlags;
}  // namespace cc

namespace blink {

// An empty WebMediaPlayer used only for tests. This class defines the methods
// of WebMediaPlayer so that mock WebMediaPlayers don't need to redefine them if
// they don't care their behavior.
class EmptyWebMediaPlayer : public WebMediaPlayer {
 public:
  ~EmptyWebMediaPlayer() override = default;

  LoadTiming Load(LoadType, const WebMediaPlayerSource&, CorsMode) override;
  void Play() override {}
  void Pause() override {}
  void Seek(double seconds) override {}
  void SetRate(double) override {}
  void SetVolume(double) override {}
  void SetLatencyHint(double) override {}
  void OnRequestPictureInPicture() override {}
  SurfaceLayerMode GetVideoSurfaceLayerMode() const override {
    return SurfaceLayerMode::kNever;
  }
  WebTimeRanges Buffered() const override;
  WebTimeRanges Seekable() const override;
  void SetSinkId(const WebString& sink_id,
                 WebSetSinkIdCompleteCallback) override {}
  bool HasVideo() const override { return false; }
  bool HasAudio() const override { return false; }
  WebSize NaturalSize() const override;
  WebSize VisibleRect() const override;
  bool Paused() const override { return false; }
  bool Seeking() const override { return false; }
  double Duration() const override { return 0.0; }
  double CurrentTime() const override { return 0.0; }
  NetworkState GetNetworkState() const override { return kNetworkStateEmpty; }
  ReadyState GetReadyState() const override { return kReadyStateHaveNothing; }
  WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override { return false; }
  bool WouldTaintOrigin() const override { return false; }
  double MediaTimeForTimeValue(double time_value) const override {
    return time_value;
  }
  unsigned DecodedFrameCount() const override { return 0; }
  unsigned DroppedFrameCount() const override { return 0; }
  uint64_t AudioDecodedByteCount() const override { return 0; }
  uint64_t VideoDecodedByteCount() const override { return 0; }
  void Paint(cc::PaintCanvas*,
             const WebRect&,
             cc::PaintFlags&,
             int already_uploaded_id,
             VideoFrameUploadMetadata*) override {}
  bool HasAvailableVideoFrame() const override { return false; }
  base::WeakPtr<WebMediaPlayer> AsWeakPtr() override { return nullptr; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_EMPTY_WEB_MEDIA_PLAYER_H_

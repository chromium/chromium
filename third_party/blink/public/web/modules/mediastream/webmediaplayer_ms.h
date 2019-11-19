// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEBMEDIAPLAYER_MS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEBMEDIAPLAYER_MS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/media/webmediaplayer_delegate.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_surface_layer_bridge.h"

namespace media {
class GpuMemoryBufferVideoFramePool;
class MediaLog;
}  // namespace media

namespace cc {
class VideoLayer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace blink {
using CreateSurfaceLayerBridgeCB =
    base::OnceCallback<std::unique_ptr<WebSurfaceLayerBridge>(
        WebSurfaceLayerBridgeObserver*,
        cc::UpdateSubmissionStateCB)>;

class MediaStreamInternalFrameWrapper;
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaStreamAudioRenderer;
class WebMediaPlayerMSCompositor;
class WebMediaStreamRendererFactory;
class WebMediaStreamVideoRenderer;
class WebString;
class WebVideoFrameSubmitter;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// All calls to WebMediaPlayerMS methods must be from the main thread of
// Renderer process.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// WebMediaStreamVideoRenderer
//   provides video frames for rendering.
//
// WebMediaPlayerClient
//   WebKit client of this media player object.
class BLINK_MODULES_EXPORT WebMediaPlayerMS
    : public WebMediaStreamObserver,
      public WebMediaPlayer,
      public WebMediaPlayerDelegate::Observer,
      public WebSurfaceLayerBridgeObserver {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides WebMediaStreamVideoRenderer.
  // |delegate| must not be null.
  WebMediaPlayerMS(
      WebLocalFrame* frame,
      WebMediaPlayerClient* client,
      WebMediaPlayerDelegate* delegate,
      std::unique_ptr<media::MediaLog> media_log,
      std::unique_ptr<WebMediaStreamRendererFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
      scoped_refptr<base::TaskRunner> worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const WebString& sink_id,
      CreateSurfaceLayerBridgeCB create_bridge_callback,
      std::unique_ptr<WebVideoFrameSubmitter> submitter_,
      WebMediaPlayer::SurfaceLayerMode surface_layer_mode);

  ~WebMediaPlayerMS() override;

  WebMediaPlayer::LoadTiming Load(LoadType load_type,
                                  const WebMediaPlayerSource& source,
                                  CorsMode cors_mode) override;

  // WebSurfaceLayerBridgeObserver implementation.
  void OnWebLayerUpdated() override;
  void RegisterContentsLayer(cc::Layer* layer) override;
  void UnregisterContentsLayer(cc::Layer* layer) override;
  void OnSurfaceIdUpdated(viz::SurfaceId surface_id) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;
  void SetLatencyHint(double seconds) override;
  void OnRequestPictureInPicture() override;
  void SetSinkId(const WebString& sink_id,
                 WebSetSinkIdCompleteCallback completion_callback) override;
  void SetPreload(WebMediaPlayer::Preload preload) override;
  WebTimeRanges Buffered() const override;
  WebTimeRanges Seekable() const override;

  // Methods for painting.
  void Paint(cc::PaintCanvas* canvas,
             const WebRect& rect,
             cc::PaintFlags& flags,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override;
  media::PaintCanvasVideoRenderer* GetPaintCanvasVideoRenderer();
  void ResetCanvasCache();

  // Methods to trigger resize event.
  void TriggerResize();

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimensions of the video.
  WebSize NaturalSize() const override;

  WebSize VisibleRect() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  double CurrentTime() const override;

  // Internal states of loading and network.
  WebMediaPlayer::NetworkState GetNetworkState() const override;
  WebMediaPlayer::ReadyState GetReadyState() const override;

  WebMediaPlayer::SurfaceLayerMode GetVideoSurfaceLayerMode() const override;

  WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override;

  bool WouldTaintOrigin() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  uint64_t AudioDecodedByteCount() const override;
  uint64_t VideoDecodedByteCount() const override;

  bool HasAvailableVideoFrame() const override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameClosed() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override;
  void OnPlay() override;
  void OnPause() override;
  void OnMuted(bool muted) override;
  void OnSeekForward(double seconds) override;
  void OnSeekBackward(double seconds) override;
  void OnVolumeMultiplierUpdate(double multiplier) override;
  void OnBecamePersistentVideo(bool value) override;

  void OnFirstFrameReceived(media::VideoRotation video_rotation,
                            bool is_opaque);
  void OnOpacityChanged(bool is_opaque);
  void OnRotationChanged(media::VideoRotation video_rotation);

  bool CopyVideoTextureToPlatformTexture(
      gpu::gles2::GLES2Interface* gl,
      unsigned target,
      unsigned int texture,
      unsigned internal_format,
      unsigned format,
      unsigned type,
      int level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata) override;

  bool CopyVideoYUVDataToPlatformTexture(
      gpu::gles2::GLES2Interface* gl,
      unsigned target,
      unsigned int texture,
      unsigned internal_format,
      unsigned format,
      unsigned type,
      int level,
      bool premultiply_alpha,
      bool flip_y,
      int already_uploaded_id,
      VideoFrameUploadMetadata* out_metadata) override;

  bool TexImageImpl(TexImageFunctionID functionID,
                    unsigned target,
                    gpu::gles2::GLES2Interface* gl,
                    unsigned int texture,
                    int level,
                    int internalformat,
                    unsigned format,
                    unsigned type,
                    int xoffset,
                    int yoffset,
                    int zoffset,
                    bool flip_y,
                    bool premultiply_alpha) override;

  // WebMediaStreamObserver implementation
  void TrackAdded(const WebMediaStreamTrack& track) override;
  void TrackRemoved(const WebMediaStreamTrack& track) override;
  void ActiveStateChanged(bool is_active) override;
  int GetDelegateId() override;
  base::Optional<viz::SurfaceId> GetSurfaceId() override;

  base::WeakPtr<WebMediaPlayer> AsWeakPtr() override;

  void OnDisplayTypeChanged(WebMediaPlayer::DisplayType) override;

 private:
  friend class WebMediaPlayerMSTest;

#if defined(OS_WIN)
  static const gfx::Size kUseGpuMemoryBufferVideoFramesMinResolution;
#endif  // defined(OS_WIN)

  bool IsInPictureInPicture() const;

  // Switch to SurfaceLayer, either initially or from VideoLayer.
  void ActivateSurfaceLayerForVideo();

  // Need repaint due to state change.
  void RepaintInternal();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(WebMediaPlayer::NetworkState state);
  void SetReadyState(WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  WebMediaPlayerClient* get_client() { return client_; }

  // To be run when tracks are added or removed.
  void Reload();
  void ReloadVideo();
  void ReloadAudio();

  // Helper method used for testing.
  void SetGpuMemoryBufferVideoForTesting(
      media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool);

  std::unique_ptr<MediaStreamInternalFrameWrapper> internal_frame_;

  WebMediaPlayer::NetworkState network_state_;
  WebMediaPlayer::ReadyState ready_state_;

  const WebTimeRanges buffered_;

  WebMediaPlayerClient* const client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after
  // registration.
  //
  // NOTE: HTMLMediaElement is a PausableObject, and will receive a
  // call to contextDestroyed() when Document::shutdown() is called.
  // Document::shutdown() is called before the frame detaches (and before the
  // frame is destroyed). RenderFrameImpl owns of |delegate_|, and is guaranteed
  // to outlive |this|. It is therefore safe use a raw pointer directly.
  WebMediaPlayerDelegate* delegate_;
  int delegate_id_;

  // Inner class used for transfering frames on compositor thread to
  // |compositor_|.
  class FrameDeliverer;
  std::unique_ptr<FrameDeliverer> frame_deliverer_;

  scoped_refptr<WebMediaStreamVideoRenderer> video_frame_provider_;  // Weak

  scoped_refptr<cc::VideoLayer> video_layer_;

  scoped_refptr<WebMediaStreamAudioRenderer> audio_renderer_;  // Weak
  media::PaintCanvasVideoRenderer video_renderer_;

  bool paused_;
  media::VideoTransformation video_transformation_;

  std::unique_ptr<media::MediaLog> media_log_;

  std::unique_ptr<WebMediaStreamRendererFactory> renderer_factory_;

  const scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  const scoped_refptr<base::TaskRunner> worker_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  THREAD_CHECKER(thread_checker_);

  scoped_refptr<WebMediaPlayerMSCompositor> compositor_;

  const WebString initial_audio_output_device_id_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction
  // (ducking) for a transient sound.  Playout volume is derived by volume *
  // multiplier.
  double volume_;
  double volume_multiplier_;

  // True if playback should be started upon the next call to OnShown(). Only
  // used on Android.
  bool should_play_upon_shown_;
  WebMediaStream web_stream_;
  // IDs of the tracks currently played.
  WebString current_video_track_id_;
  WebString current_audio_track_id_;

  CreateSurfaceLayerBridgeCB create_bridge_callback_;

  std::unique_ptr<WebVideoFrameSubmitter> submitter_;

  // Whether the use of a surface layer instead of a video layer is enabled.
  WebMediaPlayer::SurfaceLayerMode surface_layer_mode_ =
      WebMediaPlayer::SurfaceLayerMode::kNever;

  // Owns the weblayer and obtains/maintains SurfaceIds for
  // kUseSurfaceLayerForVideo feature.
  std::unique_ptr<WebSurfaceLayerBridge> bridge_;

  // Whether the video is known to be opaque or not.
  bool opaque_ = true;

  bool has_first_frame_ = false;

  base::WeakPtr<WebMediaPlayerMS> weak_this_;
  base::WeakPtrFactory<WebMediaPlayerMS> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEBMEDIAPLAYER_MS_H_

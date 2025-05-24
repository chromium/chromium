// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_

#include <d3d11.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <wrl.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "base/win/windows_types.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/media_resource.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"
#include "media/base/renderer_client.h"
#include "media/renderers/win/media_engine_extension.h"
#include "media/renderers/win/media_engine_notify_impl.h"
#include "media/renderers/win/media_foundation_protection_manager.h"
#include "media/renderers/win/media_foundation_renderer_extension.h"
#include "media/renderers/win/media_foundation_source_wrapper.h"
#include "media/renderers/win/media_foundation_texture_pool.h"

namespace media {

class MediaLog;

// MediaFoundationRenderer bridges the Renderer and Windows MFMediaEngine
// interfaces.
class MEDIA_EXPORT MediaFoundationRenderer
    : public Renderer,
      public MediaFoundationRendererExtension {
 public:
  // An enum for recording MediaFoundationRenderer playback error reason.
  // Reported to UMA. Do not change existing values.
  // Updates to ErrorReason also requires the changes updated to
  // tools/metrics/histograms/metadata/media/enums.xml.
  enum class ErrorReason {
    kUnknown = 0,
    kCdmProxyReceivedInInvalidState = 1,
    kFailedToSetSourceOnMediaEngine = 2,
    kFailedToSetCurrentTime = 3,
    kFailedToPlay = 4,
    kOnPlaybackError = 5,
    kOnDCompSurfaceReceivedError [[deprecated]] = 6,
    kOnDCompSurfaceHandleSetError = 7,
    kOnConnectionError = 8,
    kFailedToSetDCompMode = 9,
    kFailedToGetDCompSurface = 10,
    kFailedToDuplicateHandle = 11,
    kFailedToCreateMediaEngine = 12,
    kFailedToCreateDCompTextureWrapper = 13,
    kFailedToInitDCompTextureWrapper = 14,
    kFailedToSetPlaybackRate = 15,
    kFailedToGetMediaEngineEx = 16,
    // Add new values here and update `kMaxValue`. Never reuse existing values.
    kMaxValue = kFailedToGetMediaEngineEx,
  };

  // Report `reason` to UMA.
  static void ReportErrorReason(ErrorReason reason);

  MediaFoundationRenderer(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          std::unique_ptr<MediaLog> media_log,
                          LUID gpu_process_adapter_luid,
                          bool force_dcomp_mode_for_testing = false);
  MediaFoundationRenderer(const MediaFoundationRenderer&) = delete;
  MediaFoundationRenderer& operator=(const MediaFoundationRenderer&) = delete;
  ~MediaFoundationRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // MediaFoundationRendererExtension implementation.
  void GetDCompSurface(GetDCompSurfaceCB callback) override;
  void SetVideoStreamEnabled(bool enabled) override;
  void SetOutputRect(const gfx::Rect& output_rect,
                     SetOutputRectCB callback) override;
  void NotifyFrameReleased(const base::UnguessableToken& frame_token) override;
  void RequestNextFrame() override;
  void SetMediaFoundationRenderingMode(
      MediaFoundationRenderingMode render_mode) override;

  using FrameReturnCallback = base::RepeatingCallback<
      void(const base::UnguessableToken&, const gfx::Size&, base::TimeDelta)>;
  void SetFrameReturnCallbacks(
      FrameReturnCallback frame_available_cb,
      FramePoolInitializedCallback initialized_frame_pool_cb);
  void SetGpuProcessAdapterLuid(LUID gpu_process_adapter_luid);

  // Testing verification
  bool InFrameServerMode();

 private:
  HRESULT CreateMediaEngine(MediaResource* media_resource);
  HRESULT InitializeDXGIDeviceManager();
  HRESULT InitializeVirtualVideoWindow();

  // Update RendererClient with rendering statistics periodically.
  HRESULT PopulateStatistics(PipelineStatistics& statistics);
  void SendStatistics();
  void StartSendingStatistics();
  void StopSendingStatistics();

  // Callbacks for `mf_media_engine_notify_`.
  void OnPlaybackError(PipelineStatus status, HRESULT hr);
  void OnPlaybackEnded();
  void OnFormatChange();
  void OnLoadedData();
  void OnCanPlayThrough();
  void OnPlaying();
  void OnWaiting();
  void OnFrameStepCompleted();
  void OnTimeUpdate();

  // Callback for `content_protection_manager_`.
  void OnProtectionManagerWaiting(WaitingReason reason);

  void OnCdmProxyReceived();
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason);

  HRESULT SetDCompModeInternal();
  HRESULT GetDCompSurfaceInternal(HANDLE* surface_handle);
  HRESULT SetSourceOnMediaEngine();
  HRESULT UpdateVideoStream(const gfx::Size rect_size);
  HRESULT PauseInternal();
  HRESULT InitializeTexturePool(const gfx::Size& size);
  void OnVideoNaturalSizeChange();

  // Handles errors in MediaFoundationRenderer:
  // - DLOG for local debugging
  // - MEDIA_LOG for media-internals, dev tools etc.
  // - Report error reason to UMA.
  // - Notify the `cdm_proxy_`.
  // - Notify the client via `status_cb`, or if `status_cb` is null, notify
  //   `renderer_client` via OnError().
  void OnError(PipelineStatus status,
               ErrorReason reason,
               HRESULT hresult,
               PipelineStatusCallback status_cb = base::NullCallback());

  // Renderer methods are running in the same sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to report media logs. Can be called on any thread.
  std::unique_ptr<MediaLog> media_log_;

  // LUID identifying the graphics adapter used by the GPU process, the DXGI
  // device created for Media Foundation Renderer must match in order to share
  // handles between the two processes for Frame Server mode.
  LUID gpu_process_adapter_luid_;

  // Once set, will force `mf_media_engine_` to use DirectComposition mode.
  // This is used for testing.
  const bool force_dcomp_mode_for_testing_;

  raw_ptr<RendererClient> renderer_client_;
  FrameReturnCallback frame_available_cb_;
  FramePoolInitializedCallback initialized_frame_pool_cb_;

  Microsoft::WRL::ComPtr<IMFMediaEngine> mf_media_engine_;
  Microsoft::WRL::ComPtr<MediaEngineNotifyImpl> mf_media_engine_notify_;
  Microsoft::WRL::ComPtr<MediaEngineExtension> mf_media_engine_extension_;
  Microsoft::WRL::ComPtr<MediaFoundationSourceWrapper> mf_source_;

  // This enables MFMediaEngine to use hardware acceleration for video decoding
  // and video processing.
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_device_manager_;

  // Current cached rectangle size of video to be rendered.
  gfx::Size current_video_rect_size_;

  // Current duration of the media.
  base::TimeDelta duration_;

  // This is the same as "natural_size" in Chromium.
  gfx::Size native_video_size_;

  // Keep the last volume value being set.
  float volume_ = 1.0;

  // Current playback rate.
  double playback_rate_ = 0.0;

  // Used for RendererClient::OnBufferingStateChange().
  BufferingState max_buffering_state_ = BufferingState::BUFFERING_HAVE_NOTHING;

  // Used for RendererClient::OnStatisticsUpdate().
  PipelineStatistics statistics_ = {};
  base::RepeatingTimer statistics_timer_;

  // Tracks the number of MEDIA_LOGs emitted for failure to populate statistics.
  // Useful to prevent log spam.
  int populate_statistics_failure_count_ = 0;

  // A fake window handle passed to MF-based rendering pipeline for OPM.
  HWND virtual_video_window_ = nullptr;

  bool waiting_for_mf_cdm_ = false;
  raw_ptr<CdmContext> cdm_context_ = nullptr;
  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;

  Microsoft::WRL::ComPtr<MediaFoundationProtectionManager>
      content_protection_manager_;

  // Texture pool of ID3D11Texture2D for the media engine to draw video frames
  // when the media engine is in frame server mode instead of Direct
  // Composition mode.
  MediaFoundationTexturePool texture_pool_;

  // Rendering mode the Media Engine will use.
  MediaFoundationRenderingMode rendering_mode_ =
      MediaFoundationRenderingMode::DirectComposition;

  bool has_reported_playing_ = false;
  bool has_reported_significant_playback_ = false;

  // Value saved from last call to SetLatencyHint(). Latency hint can only be
  // used to determine real-time mode on MediaEngine creation.
  // IMFMediaEngineEx::SetRealTimeMode is only applicable to the next
  // IMFMediaEngine::SetSource call so we aren't able to change real-time mode
  // dynamically in MFR use cases.
  std::optional<base::TimeDelta> latency_hint_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_

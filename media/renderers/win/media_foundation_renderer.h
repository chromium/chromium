// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_

#include <d3d11.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <wrl.h>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
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

namespace media {

class MediaLog;

// MediaFoundationRenderer bridges the Renderer and Windows MFMediaEngine
// interfaces.
class MEDIA_EXPORT MediaFoundationRenderer
    : public Renderer,
      public MediaFoundationRendererExtension {
 public:
  // Whether MediaFoundationRenderer() is supported on the current device.
  static bool IsSupported();

  MediaFoundationRenderer(scoped_refptr<base::SequencedTaskRunner> task_runner,
                          std::unique_ptr<MediaLog> media_log,
                          bool force_dcomp_mode_for_testing = false);
  MediaFoundationRenderer(const MediaFoundationRenderer&) = delete;
  MediaFoundationRenderer& operator=(const MediaFoundationRenderer&) = delete;
  ~MediaFoundationRenderer() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

  // MediaFoundationRendererExtension implementation.
  void GetDCompSurface(GetDCompSurfaceCB callback) override;
  void SetVideoStreamEnabled(bool enabled) override;
  void SetOutputRect(const gfx::Rect& output_rect,
                     SetOutputRectCB callback) override;

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
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason);
  void OnVideoNaturalSizeChange();
  void OnTimeUpdate();

  // Callback for `content_protection_manager_`.
  void OnWaiting(WaitingReason reason);

  void OnCdmProxyReceived(scoped_refptr<MediaFoundationCdmProxy> cdm_proxy);

  HRESULT SetDCompModeInternal();
  HRESULT GetDCompSurfaceInternal(HANDLE* surface_handle);
  HRESULT SetSourceOnMediaEngine();
  HRESULT UpdateVideoStream(const gfx::Rect& rect);

  // Renderer methods are running in the same sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to report media logs. Can be called on any thread.
  std::unique_ptr<MediaLog> media_log_;

  // Once set, will force `mf_media_engine_` to use DirectComposition mode.
  // This is used for testing.
  const bool force_dcomp_mode_for_testing_;

  RendererClient* renderer_client_;

  Microsoft::WRL::ComPtr<IMFMediaEngine> mf_media_engine_;
  Microsoft::WRL::ComPtr<MediaEngineNotifyImpl> mf_media_engine_notify_;
  Microsoft::WRL::ComPtr<MediaEngineExtension> mf_media_engine_extension_;
  Microsoft::WRL::ComPtr<MediaFoundationSourceWrapper> mf_source_;
  // This enables MFMediaEngine to use hardware acceleration for video decoding
  // and vdieo processing.
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgi_device_manager_;

  // Current duration of the media.
  base::TimeDelta duration_;

  // This is the same as "natural_size" in Chromium.
  gfx::Size native_video_size_;

  // Keep the last volume value being set.
  float volume_ = 1.0;

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
  CdmContext* cdm_context_ = nullptr;
  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;

  Microsoft::WRL::ComPtr<MediaFoundationProtectionManager>
      content_protection_manager_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_

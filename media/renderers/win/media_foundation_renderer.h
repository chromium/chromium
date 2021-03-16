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
#include "base/macros.h"
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
#include "media/base/win/mf_initializer.h"
#include "media/renderers/win/media_engine_extension.h"
#include "media/renderers/win/media_engine_notify_impl.h"
#include "media/renderers/win/media_foundation_protection_manager.h"
#include "media/renderers/win/media_foundation_renderer_extension.h"
#include "media/renderers/win/media_foundation_source_wrapper.h"

namespace media {

// MediaFoundationRenderer bridges the Renderer and Windows MFMediaEngine
// interfaces.
class MEDIA_EXPORT MediaFoundationRenderer
    : public Renderer,
      public MediaFoundationRendererExtension {
 public:
  // Whether MediaFoundationRenderer() is supported on the current device.
  static bool IsSupported();

  MediaFoundationRenderer(bool muted,
                          scoped_refptr<base::SequencedTaskRunner> task_runner,
                          bool force_dcomp_mode_for_testing = false);

  ~MediaFoundationRenderer() override;

  // TODO(frankli): naming: Change DComp into DirectComposition for interface
  // method names in a separate CL.

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;

  // MediaFoundationRendererExtension implementation.
  void SetDCompMode(bool enabled, SetDCompModeCB callback) override;
  void GetDCompSurface(GetDCompSurfaceCB callback) override;
  void SetVideoStreamEnabled(bool enabled) override;
  void SetOutputParams(const gfx::Rect& output_rect) override;

 private:
  HRESULT CreateMediaEngine(MediaResource* media_resource);
  HRESULT InitializeDXGIDeviceManager();
  HRESULT InitializeVirtualVideoWindow();

  // Update RendererClient with rendering statistics periodically.
  HRESULT PopulateStatistics(PipelineStatistics& statistics);
  void SendStatistics();
  void StartSendingStatistics();
  void StopSendingStatistics();

  // Callbacks for |mf_media_engine_notify_|.
  void OnPlaybackError(PipelineStatus status);
  void OnPlaybackEnded();
  void OnBufferingStateChange(BufferingState state,
                              BufferingStateChangeReason reason);
  void OnVideoNaturalSizeChange();
  void OnTimeUpdate();

  void OnCdmProxyReceived(Microsoft::WRL::ComPtr<IMFCdmProxy> cdm_proxy);

  HRESULT SetDCompModeInternal(bool enabled);
  HRESULT GetDCompSurfaceInternal(HANDLE* surface_handle);
  HRESULT SetSourceOnMediaEngine();
  HRESULT SetOutputParamsInternal(const gfx::Rect& output_rect);

  // TODO(crbug.com/1017943): Support Audio Indicator when using
  // media::MojoRenderer. For now, keep |muted_| as const.
  const bool muted_;

  // Renderer methods are running in the same sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Once set, will force |mf_media_engine_| to use DirectComposition mode.
  // This is used for testing.
  const bool force_dcomp_mode_for_testing_;

  // Keep this here so it's destroyed after all Media Foundation members below.
  MFSessionLifetime mf_session_life_time_;

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

  // A fake window handle passed to MF-based rendering pipeline for OPM.
  HWND virtual_video_window_ = nullptr;

  base::UnguessableToken surface_request_token_;
  base::win::ScopedHandle dcomp_surface_handle_;

  bool waiting_for_mf_cdm_ = false;
  CdmContext* cdm_context_ = nullptr;
  Microsoft::WRL::ComPtr<MediaFoundationProtectionManager>
      content_protection_manager_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationRenderer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaFoundationRenderer);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERER_H_

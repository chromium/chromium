// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_DECRYPTING_RENDERER_H_
#define MEDIA_RENDERERS_DECRYPTING_RENDERER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "media/base/pipeline.h"
#include "media/base/renderer.h"

namespace media {

class CdmContext;
class DemuxerStream;
class MediaLog;
class MediaResource;
class DecryptingMediaResource;
class RendererClient;

// DecryptingRenderer is used as a wrapper around a Renderer
// implementation that decrypts streams when an AesDecryptor is available. In
// this case only clear streams are passed on to the internally owned renderer
// implementation.
//
// All methods are pass-through except Initialize() and SetCdm().
//
// The caller must guarantee that DecryptingRenderer will never be initialized
// with a |media_resource| of type MediaResource::Type::URL.
class MEDIA_EXPORT DecryptingRenderer : public Renderer {
 public:
  DecryptingRenderer(
      std::unique_ptr<Renderer> renderer,
      MediaLog* media_log,
      const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  ~DecryptingRenderer() override;

  // Renderer implementation:
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(base::Optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;

  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  void OnSelectedVideoTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;
  void OnEnabledAudioTracksChanged(
      const std::vector<DemuxerStream*>& enabled_tracks,
      base::OnceClosure change_completed_cb) override;

  bool HasDecryptingMediaResourceForTesting() const;

 private:
  friend class DecryptingRendererTest;

  // Cannot be called before Initialize() has been called.
  void CreateAndInitializeDecryptingMediaResource();

  // Invoked as a callback after |decrypting_media_resource_| has been
  // initialized.
  void InitializeRenderer(bool success);
  bool HasEncryptedStream();
  void OnWaiting(WaitingReason reason);

  const std::unique_ptr<Renderer> renderer_;
  MediaLog* const media_log_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  bool waiting_for_cdm_ = false;
  CdmContext* cdm_context_ = nullptr;
  RendererClient* client_;
  MediaResource* media_resource_;
  PipelineStatusCallback init_cb_;

  std::unique_ptr<DecryptingMediaResource> decrypting_media_resource_;

  base::WeakPtrFactory<DecryptingRenderer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DecryptingRenderer);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_DECRYPTING_RENDERER_H_

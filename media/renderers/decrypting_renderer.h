// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_DECRYPTING_RENDERER_H_
#define MEDIA_RENDERERS_DECRYPTING_RENDERER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
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
      const scoped_refptr<base::SequencedTaskRunner> media_task_runner);

  DecryptingRenderer(const DecryptingRenderer&) = delete;
  DecryptingRenderer& operator=(const DecryptingRenderer&) = delete;

  ~DecryptingRenderer() override;

  // Renderer implementation:
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(std::optional<base::TimeDelta> latency_hint) override;
  void SetPreservesPitch(bool preserves_pitch) override;
  void SetWasPlayedWithUserActivationAndHighMediaEngagement(
      bool was_played_with_user_activation_and_high_media_engagement) override;

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
  RendererType GetRendererType() override;

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
  const raw_ptr<MediaLog> media_log_;
  const scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  bool waiting_for_cdm_ = false;
  raw_ptr<CdmContext> cdm_context_ = nullptr;
  raw_ptr<RendererClient> client_;
  raw_ptr<MediaResource> media_resource_;
  PipelineStatusCallback init_cb_;

  std::unique_ptr<DecryptingMediaResource> decrypting_media_resource_;

  base::WeakPtrFactory<DecryptingRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_RENDERERS_DECRYPTING_RENDERER_H_

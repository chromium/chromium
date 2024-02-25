// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_ENCODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_ENCODER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "media/base/audio_encoder.h"
#include "media/base/status.h"
#include "media/mojo/mojom/audio_encoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace media {

class MEDIA_MOJO_EXPORT MojoAudioEncoderService final
    : public mojom::AudioEncoder {
 public:
  explicit MojoAudioEncoderService(
      std::unique_ptr<media::AudioEncoder> encoder);

  MojoAudioEncoderService(const MojoAudioEncoderService&) = delete;
  MojoAudioEncoderService& operator=(const MojoAudioEncoderService&) = delete;

  ~MojoAudioEncoderService() final;

  // mojom::AudioEncoder implementation
  void Initialize(
      mojo::PendingAssociatedRemote<mojom::AudioEncoderClient> client,
      const AudioEncoderConfig& config,
      InitializeCallback callback) final;

  void Encode(mojom::AudioBufferPtr buffer, EncodeCallback callback) final;

  void Flush(FlushCallback callback) final;

 private:
  using MojoDoneCallback =
      base::OnceCallback<void(const media::EncoderStatus&)>;
  void OnDone(MojoDoneCallback callback, EncoderStatus error);
  void OnOutput(EncodedAudioBuffer output,
                std::optional<media::AudioEncoder::CodecDescription> desc);

  std::unique_ptr<media::AudioEncoder> encoder_;
  mojo::AssociatedRemote<mojom::AudioEncoderClient> client_;

  base::WeakPtr<MojoAudioEncoderService> weak_this_;
  base::WeakPtrFactory<MojoAudioEncoderService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_ENCODER_SERVICE_H_

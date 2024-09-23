// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_context.h"
#include "media/base/status.h"
#include "media/mojo/mojom/audio_decoder.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class MojoCdmServiceContext;
class MojoDecoderBufferReader;
class MojoMediaClient;

class MEDIA_MOJO_EXPORT MojoAudioDecoderService final
    : public mojom::AudioDecoder {
 public:
  MojoAudioDecoderService(
      MojoMediaClient* mojo_media_client,
      MojoCdmServiceContext* mojo_cdm_service_context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  MojoAudioDecoderService(const MojoAudioDecoderService&) = delete;
  MojoAudioDecoderService& operator=(const MojoAudioDecoderService&) = delete;

  ~MojoAudioDecoderService() final;

  // mojom::AudioDecoder implementation
  void GetSupportedConfigs(GetSupportedConfigsCallback callback) final;
  void Construct(
      mojo::PendingAssociatedRemote<mojom::AudioDecoderClient> client,
      mojo::PendingRemote<mojom::MediaLog> media_log) final;
  void Initialize(const AudioDecoderConfig& config,
                  const std::optional<base::UnguessableToken>& cdm_id,
                  InitializeCallback callback) final;

  void SetDataSource(mojo::ScopedDataPipeConsumerHandle receive_pipe) final;

  void Decode(mojom::DecoderBufferPtr buffer, DecodeCallback callback) final;

  void Reset(ResetCallback callback) final;

 private:
  // Called by |decoder_| upon finishing initialization.
  void OnInitialized(InitializeCallback callback, DecoderStatus status);

  // Called by |mojo_decoder_buffer_reader_| when read is finished.
  void OnReadDone(mojo::ReportBadMessageCallback bad_message_callback,
                  DecodeCallback callback,
                  scoped_refptr<DecoderBuffer> buffer);

  // Called by |mojo_decoder_buffer_reader_| when reset is finished.
  void OnReaderFlushDone(ResetCallback callback);

  // Called by |decoder_| when DecoderBuffer is accepted or rejected.
  void OnDecodeStatus(DecodeCallback callback, DecoderStatus status);

  // Called by |decoder_| when reset sequence is finished.
  void OnResetDone(ResetCallback callback);

  // Called by |decoder_| for each decoded buffer.
  void OnAudioBufferReady(scoped_refptr<AudioBuffer> audio_buffer);

  // Called by |decoder_| when it's waiting because of |reason|, e.g. waiting
  // for decryption key.
  void OnWaiting(WaitingReason reason);

  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  const raw_ptr<MojoMediaClient> mojo_media_client_;

  // A helper object required to get CDM from CDM id.
  const raw_ptr<MojoCdmServiceContext> mojo_cdm_service_context_ = nullptr;

  // The destination for the decoded buffers.
  mojo::AssociatedRemote<mojom::AudioDecoderClient> client_;

  // The CDM ID and the corresponding CdmContextRef, which must be held to keep
  // the CdmContext alive for the lifetime of the |decoder_|.
  std::optional<base::UnguessableToken> cdm_id_;
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  // The AudioDecoder that does actual decoding work.
  // This MUST be declared after |cdm_| to maintain correct destruction order.
  // The |decoder_| may need to access the CDM to do some clean up work in its
  // own destructor.
  std::unique_ptr<media::AudioDecoder> decoder_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtr<MojoAudioDecoderService> weak_this_;
  base::WeakPtrFactory<MojoAudioDecoderService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_SERVICE_H_

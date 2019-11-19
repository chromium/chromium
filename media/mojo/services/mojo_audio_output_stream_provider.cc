// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream_provider.h"

#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/bindings/message.h"

namespace media {

MojoAudioOutputStreamProvider::MojoAudioOutputStreamProvider(
    mojo::PendingReceiver<mojom::AudioOutputStreamProvider> pending_receiver,
    CreateDelegateCallback create_delegate_callback,
    DeleterCallback deleter_callback,
    std::unique_ptr<media::mojom::AudioOutputStreamObserver> observer)
    : receiver_(this, std::move(pending_receiver)),
      create_delegate_callback_(std::move(create_delegate_callback)),
      deleter_callback_(std::move(deleter_callback)),
      observer_(std::move(observer)),
      observer_receiver_(observer_.get()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Unretained is safe since |this| owns |receiver_|.
  receiver_.set_disconnect_handler(
      base::BindOnce(&MojoAudioOutputStreamProvider::CleanUp,
                     base::Unretained(this), /*had_error*/ false));
  DCHECK(create_delegate_callback_);
  DCHECK(deleter_callback_);
}

MojoAudioOutputStreamProvider::~MojoAudioOutputStreamProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioOutputStreamProvider::Acquire(
    const AudioParameters& params,
    mojo::PendingRemote<mojom::AudioOutputStreamProviderClient> provider_client,
    const base::Optional<base::UnguessableToken>& processing_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
// |processing_id| gets dropped here. It's not supported outside of the audio
// service. As this class is slated for removal, it will not be updated to
// support audio processing.
#if !defined(OS_ANDROID)
  if (params.IsBitstreamFormat()) {
    // Bitstream streams are only supported on Android.
    BadMessage(
        "Attempted to acquire a bitstream audio stream on a platform where "
        "it's not supported");
    return;
  }
#endif
  if (audio_output_) {
    BadMessage("Output acquired twice.");
    return;
  }

  provider_client_.Bind(std::move(provider_client));

  mojo::PendingRemote<mojom::AudioOutputStreamObserver> pending_observer;
  observer_receiver_.Bind(pending_observer.InitWithNewPipeAndPassReceiver());
  // Unretained is safe since |this| owns |audio_output_|.
  audio_output_.emplace(
      base::BindOnce(std::move(create_delegate_callback_), params,
                     std::move(pending_observer)),
      base::BindOnce(&mojom::AudioOutputStreamProviderClient::Created,
                     base::Unretained(provider_client_.get())),
      base::BindOnce(&MojoAudioOutputStreamProvider::CleanUp,
                     base::Unretained(this)));
}

void MojoAudioOutputStreamProvider::CleanUp(bool had_error) {
  if (had_error) {
    provider_client_.ResetWithReason(
        static_cast<uint32_t>(media::mojom::AudioOutputStreamObserver::
                                  DisconnectReason::kPlatformError),
        std::string());
  }
  std::move(deleter_callback_).Run(this);
}

void MojoAudioOutputStreamProvider::BadMessage(const std::string& error) {
  mojo::ReportBadMessage(error);
  std::move(deleter_callback_).Run(this);  // deletes |this|.
}

}  // namespace media

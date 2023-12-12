// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_H_

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"

namespace media {
class AudioSystem;
}

namespace content {

// Returns the browser's main control interface into the Audio Service, which
// is started lazily and may run either in-process or in a dedicated sandboxed
// subprocess.
CONTENT_EXPORT audio::mojom::AudioService& GetAudioService();

// Provides an override for the reference returned by
// |GetAudioService()|.
CONTENT_EXPORT base::AutoReset<audio::mojom::AudioService*>
OverrideAudioServiceForTesting(audio::mojom::AudioService* service);

// Creates an instance of AudioSystem for use with the Audio Service, bound to
// the thread it's used on for the first time.
CONTENT_EXPORT std::unique_ptr<media::AudioSystem>
CreateAudioSystemForAudioService();

// Returns a callback that can be invoked from any sequence to safely bind a
// AudioStreamFactory interface receiver in the Audio Service.
using AudioServiceStreamFactoryBinder = base::RepeatingCallback<void(
    mojo::PendingReceiver<media::mojom::AudioStreamFactory>)>;
CONTENT_EXPORT AudioServiceStreamFactoryBinder
GetAudioServiceStreamFactoryBinder();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_H_

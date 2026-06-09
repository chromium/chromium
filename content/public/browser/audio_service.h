// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_AUDIO_SERVICE_H_

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/observed_service_remote.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/audio/public/mojom/audio_service.mojom.h"

namespace media {
class AudioSystem;
}

namespace content {

using AudioServiceProcessObserver =
    ObservedServiceRemote<audio::mojom::AudioService>::Observer;

// If the audio service is already running, the observer will be immediately
// notified via OnServiceLaunched. Must be called on the UI thread.
CONTENT_EXPORT void AddAudioServiceProcessObserver(
    AudioServiceProcessObserver* observer);

// Must be called before the observer is destroyed. UI thread only.
CONTENT_EXPORT void RemoveAudioServiceProcessObserver(
    AudioServiceProcessObserver* observer);

// Returns the browser's main control interface into the Audio Service, which
// is started lazily and may run either in-process or in a dedicated sandboxed
// subprocess.
CONTENT_EXPORT audio::mojom::AudioService& GetAudioService();

// Provides an override for the reference returned by
// |GetAudioService()|.
CONTENT_EXPORT base::AutoReset<audio::mojom::AudioService*>
OverrideAudioServiceForTesting(audio::mojom::AudioService* service);

// Resets the audio service remote state so that a subsequent call to
// GetAudioService() will re-launch. Call this during test teardown when
// the task environment is being destroyed and recreated between tests.
CONTENT_EXPORT void ResetAudioServiceForTesting();

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

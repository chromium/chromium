// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AudioInputDeviceManager manages the audio input devices. In particular it
// communicates with MediaStreamManager and RenderFrameAudioInputStreamFactory
// on the browser IO thread, handles queries like
// enumerate/open/close/GetOpenedDeviceById from MediaStreamManager and
// GetOpenedDeviceById from RenderFrameAudioInputStreamFactory.
// The work for enumerate/open/close is handled asynchronously on Media Stream
// device thread, while GetOpenedDeviceById is synchronous on the IO thread.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"

namespace media {
class AudioSystem;
}

namespace content {

// Should be used on IO thread only.
class CONTENT_EXPORT AudioInputDeviceManager : public MediaStreamProvider {
 public:
  explicit AudioInputDeviceManager(media::AudioSystem* audio_system);

  AudioInputDeviceManager(const AudioInputDeviceManager&) = delete;
  AudioInputDeviceManager& operator=(const AudioInputDeviceManager&) = delete;

  // Gets the opened device by |session_id|. Returns NULL if the device
  // is not opened, otherwise the opened device. Called on IO thread.
  const blink::MediaStreamDevice* GetOpenedDeviceById(
      const base::UnguessableToken& session_id);

  // MediaStreamProvider implementation.
  void RegisterListener(MediaStreamProviderListener* listener) override;
  void UnregisterListener(MediaStreamProviderListener* listener) override;
  base::UnguessableToken Open(const blink::MediaStreamDevice& device) override;
  void Close(const base::UnguessableToken& session_id) override;

 private:
  ~AudioInputDeviceManager() override;

  // Callback called on IO thread when device is opened.
  void OpenedOnIOThread(
      const base::UnguessableToken& session_id,
      const blink::MediaStreamDevice& device,
      const std::optional<media::AudioParameters>& input_params,
      const std::optional<std::string>& matched_output_device_id);

  // Callback called on IO thread with the session_id referencing the closed
  // device.
  void ClosedOnIOThread(blink::mojom::MediaStreamType type,
                        const base::UnguessableToken& session_id);

  // Helper to return iterator to the device referenced by |session_id|. If no
  // device is found, it will return devices_.end().
  blink::MediaStreamDevices::iterator GetDevice(
      const base::UnguessableToken& session_id);

  // Only accessed on Browser::IO thread.
  base::ObserverList<MediaStreamProviderListener>::Unchecked listeners_;
  blink::MediaStreamDevices devices_;

  const raw_ptr<media::AudioSystem> audio_system_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

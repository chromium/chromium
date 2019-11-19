// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
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

  // Gets the opened device by |session_id|. Returns NULL if the device
  // is not opened, otherwise the opened device. Called on IO thread.
  const blink::MediaStreamDevice* GetOpenedDeviceById(
      const base::UnguessableToken& session_id);

  // MediaStreamProvider implementation.
  void RegisterListener(MediaStreamProviderListener* listener) override;
  void UnregisterListener(MediaStreamProviderListener* listener) override;
  base::UnguessableToken Open(const blink::MediaStreamDevice& device) override;
  void Close(const base::UnguessableToken& session_id) override;

  // Owns a keyboard mic stream registration. Dummy implementation on platforms
  // other than Chrome OS.
  class KeyboardMicRegistration {
   public:
#if defined(OS_CHROMEOS)
    // No registration.
    KeyboardMicRegistration() = default;

    KeyboardMicRegistration(KeyboardMicRegistration&& other);

    ~KeyboardMicRegistration();

   private:
    friend class AudioInputDeviceManager;

    explicit KeyboardMicRegistration(int* shared_registration_count);

    void DeregisterIfNeeded();

    // Null to indicate that there is no stream registration. This points to
    // a member of the AudioInputDeviceManager, which lives as long as the IO
    // thread, so the pointer will be valid for the lifetime of the
    // registration.
    int* shared_registration_count_ = nullptr;
#endif
  };

#if defined(OS_CHROMEOS)
  // Registers that a stream using keyboard mic has been opened or closed.
  // Keeps count of how many such streams are open and activates and
  // inactivates the keyboard mic accordingly. The (in)activation is done on the
  // UI thread and for the register case a callback must therefore be provided
  // which is called when activated. Deregistration is done when the
  // registration object is destructed or assigned to, which should only be
  // done on the IO thread.
  void RegisterKeyboardMicStream(
      base::OnceCallback<void(KeyboardMicRegistration)> callback);
#endif

 private:
  ~AudioInputDeviceManager() override;

  // Callback called on IO thread when device is opened.
  void OpenedOnIOThread(
      const base::UnguessableToken& session_id,
      const blink::MediaStreamDevice& device,
      base::TimeTicks start_time,
      const base::Optional<media::AudioParameters>& input_params,
      const base::Optional<std::string>& matched_output_device_id);

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

#if defined(OS_CHROMEOS)
  // Keeps count of how many streams are using keyboard mic.
  int keyboard_mic_streams_count_;
#endif

  media::AudioSystem* const audio_system_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputDeviceManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DEVICE_MANAGER_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/common/content_export.h"
#include "media/audio/audio_input_delegate.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {
class AudioManager;
class AudioInputController;
class AudioInputSyncWriter;
class AudioParameters;
class UserInputMonitor;
}  // namespace media

namespace blink {
struct MediaStreamDevice;
}  // namespace blink

namespace content {

class AudioInputDeviceManager;
class AudioMirroringManager;

// This class is operated on the IO thread.
class CONTENT_EXPORT AudioInputDelegateImpl : public media::AudioInputDelegate {
 public:
  class ControllerEventHandler;

  ~AudioInputDelegateImpl() override;

  static std::unique_ptr<media::AudioInputDelegate> Create(
      media::AudioManager* audio_manager,
      AudioMirroringManager* mirroring_manager,
      media::UserInputMonitor* user_input_monitor,
      int render_process_id,
      int render_frame_id,
      AudioInputDeviceManager* audio_input_device_manager,
      mojo::PendingRemote<media::mojom::AudioLog> audio_log,
      AudioInputDeviceManager::KeyboardMicRegistration
          keyboard_mic_registration,
      uint32_t shared_memory_count,
      int stream_id,
      const base::UnguessableToken& session_id,
      bool automatic_gain_control,
      const media::AudioParameters& audio_parameters,
      EventHandler* subscriber);

  // AudioInputDelegate implementation.
  int GetStreamId() override;
  void OnRecordStream() override;
  void OnSetVolume(double volume) override;
  void OnSetOutputDeviceForAec(
      const std::string& raw_output_device_id) override;

 private:
  AudioInputDelegateImpl(
      media::AudioManager* audio_manager,
      AudioMirroringManager* mirroring_manager,
      media::UserInputMonitor* user_input_monitor,
      const media::AudioParameters& audio_parameters,
      int render_process_id,
      mojo::PendingRemote<media::mojom::AudioLog> audio_log,
      AudioInputDeviceManager::KeyboardMicRegistration
          keyboard_mic_registration,
      int stream_id,
      bool automatic_gain_control,
      EventHandler* subscriber,
      const blink::MediaStreamDevice* device,
      std::unique_ptr<media::AudioInputSyncWriter> writer,
      std::unique_ptr<base::CancelableSyncSocket> foreign_socket);

  void SendCreatedNotification(bool initially_muted);
  void OnMuted(bool is_muted);
  void OnError();

  SEQUENCE_CHECKER(sequence_checker_);

  // This is the event handler |this| sends notifications to.
  EventHandler* const subscriber_;

  // |controller_event_handler_| proxies events from controller to |this|.
  // |controller_event_handler_| and |writer_| outlive |this|, see the
  // destructor for details.
  std::unique_ptr<ControllerEventHandler> controller_event_handler_;
  std::unique_ptr<media::AudioInputSyncWriter> writer_;
  std::unique_ptr<base::CancelableSyncSocket> foreign_socket_;
  mojo::Remote<media::mojom::AudioLog> const audio_log_;
  scoped_refptr<media::AudioInputController> controller_;
  const AudioInputDeviceManager::KeyboardMicRegistration
      keyboard_mic_registration_;
  const int stream_id_;
  const int render_process_id_;
  base::WeakPtrFactory<AudioInputDelegateImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioInputDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_DELEGATE_IMPL_H_

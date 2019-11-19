// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "media/audio/audio_output_delegate.h"
#include "media/mojo/mojom/audio_logging.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class MediaObserver;
}

namespace media {
class AudioManager;
class AudioOutputController;
class AudioParameters;
class AudioSyncReader;
}

namespace content {

// This class, except for the AudioOutputDelegateImpl::EventHandler
// implementation, is operated on the IO thread.
class CONTENT_EXPORT AudioOutputDelegateImpl
    : public media::AudioOutputDelegate {
 public:
  static std::unique_ptr<AudioOutputDelegate> Create(
      EventHandler* handler,
      media::AudioManager* audio_manager,
      mojo::PendingRemote<media::mojom::AudioLog> audio_log,
      MediaObserver* media_observer,
      int stream_id,
      int render_frame_id,
      int render_process_id,
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStreamObserver>
          pending_observer,
      const std::string& output_device_id);

  AudioOutputDelegateImpl(
      std::unique_ptr<media::AudioSyncReader> reader,
      std::unique_ptr<base::CancelableSyncSocket> foreign_socket,
      EventHandler* handler,
      media::AudioManager* audio_manager,
      mojo::PendingRemote<media::mojom::AudioLog> audio_log,
      MediaObserver* media_observer,
      int stream_id,
      int render_frame_id,
      int render_process_id,
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStreamObserver>
          pending_observer,
      const std::string& output_device_id);

  ~AudioOutputDelegateImpl() override;

  // AudioOutputDelegate implementation.
  int GetStreamId() override;
  void OnPlayStream() override;
  void OnPauseStream() override;
  void OnFlushStream() override;
  void OnSetVolume(double volume) override;

 private:
  class ControllerEventHandler;
  friend class AudioOutputDelegateTest;

  void SendCreatedNotification();
  void OnError();
  void UpdatePlayingState(bool playing);
  media::AudioOutputController* GetControllerForTesting() const;
  void PollAudioLevel();
  bool IsAudible() const;

  // This is the event handler which |this| send notifications to.
  EventHandler* subscriber_;
  const mojo::Remote<media::mojom::AudioLog> audio_log_;
  // |controller_event_handler_| proxies events from controller to |this|.
  // |controller_event_handler_|, and |reader_| will outlive |this|, see the
  // destructor for details.
  std::unique_ptr<ControllerEventHandler> controller_event_handler_;
  std::unique_ptr<media::AudioSyncReader> reader_;
  std::unique_ptr<base::CancelableSyncSocket> foreign_socket_;
  scoped_refptr<media::AudioOutputController> controller_;
  const int stream_id_;

  // This flag ensures that we only send OnStreamStateChanged notifications
  // and (de)register with the stream monitor when the state actually changes.
  bool playing_ = false;

  // Calls PollAudioLevel() at regular intervals while |playing_| is true.
  base::RepeatingTimer poll_timer_;
  bool is_audible_ = false;
  // |observer_| is notified about changes in the audible state of the stream.
  mojo::Remote<media::mojom::AudioOutputStreamObserver> observer_;

  base::WeakPtrFactory<AudioOutputDelegateImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDelegateImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_DELEGATE_IMPL_H_

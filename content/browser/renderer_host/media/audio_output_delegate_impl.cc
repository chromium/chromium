// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_output_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_observer.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_sync_reader.h"

namespace content {

namespace {

// Safe to call from any thread.
void AudioOutputLogMessage(int stream_id, const std::string& message) {
  std::string out_message =
      base::StringPrintf("[stream_id=%d] %s", stream_id, message.c_str());
  content::MediaStreamManager::SendMessageToNativeLog(out_message);
  DVLOG(1) << out_message;
}

}  // namespace

const float kSilenceThresholdDBFS = -72.24719896f;
// Desired polling frequency.  Note: If this is set too low, short-duration
// "blip" sounds won't be detected.  http://crbug.com/339133#c4
const int kPowerMeasurementsPerSecond = 15;

// This class trampolines callbacks from the controller to the delegate. Since
// callbacks from the controller are stopped asynchronously, this class holds
// a weak pointer to the delegate, allowing the delegate to stop callbacks
// synchronously.
class AudioOutputDelegateImpl::ControllerEventHandler
    : public media::AudioOutputController::EventHandler {
 public:
  ControllerEventHandler(base::WeakPtr<AudioOutputDelegateImpl> delegate,
                         int stream_id);

 private:
  void OnControllerCreated() override;
  void OnControllerPlaying() override;
  void OnControllerPaused() override;
  void OnControllerError() override;
  void OnLog(base::StringPiece message) override;

  base::WeakPtr<AudioOutputDelegateImpl> delegate_;
  const int stream_id_;  // Retained separately for logging.
};

AudioOutputDelegateImpl::ControllerEventHandler::ControllerEventHandler(
    base::WeakPtr<AudioOutputDelegateImpl> delegate,
    int stream_id)
    : delegate_(std::move(delegate)), stream_id_(stream_id) {}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerCreated() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateImpl::SendCreatedNotification,
                     delegate_));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerPlaying() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateImpl::UpdatePlayingState, delegate_,
                     true));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerPaused() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateImpl::UpdatePlayingState, delegate_,
                     false));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnControllerError() {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AudioOutputDelegateImpl::OnError, delegate_));
}

void AudioOutputDelegateImpl::ControllerEventHandler::OnLog(
    base::StringPiece message) {
  AudioOutputLogMessage(stream_id_, message.as_string());
}

std::unique_ptr<media::AudioOutputDelegate> AudioOutputDelegateImpl::Create(
    EventHandler* handler,
    media::AudioManager* audio_manager,
    media::mojom::AudioLogPtr audio_log,
    MediaObserver* media_observer,
    int stream_id,
    int render_frame_id,
    int render_process_id,
    const media::AudioParameters& params,
    media::mojom::AudioOutputStreamObserverPtr observer,
    const std::string& output_device_id) {
  auto socket = std::make_unique<base::CancelableSyncSocket>();
  auto reader = media::AudioSyncReader::Create(
      base::BindRepeating(&AudioOutputLogMessage, stream_id), params,
      socket.get());
  if (!reader)
    return nullptr;

  return std::make_unique<AudioOutputDelegateImpl>(
      std::move(reader), std::move(socket), handler, audio_manager,
      std::move(audio_log), media_observer, stream_id, render_frame_id,
      render_process_id, params, std::move(observer), output_device_id);
}

AudioOutputDelegateImpl::AudioOutputDelegateImpl(
    std::unique_ptr<media::AudioSyncReader> reader,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket,
    EventHandler* handler,
    media::AudioManager* audio_manager,
    media::mojom::AudioLogPtr audio_log,
    MediaObserver* media_observer,
    int stream_id,
    int render_frame_id,
    int render_process_id,
    const media::AudioParameters& params,
    media::mojom::AudioOutputStreamObserverPtr observer,
    const std::string& output_device_id)
    : subscriber_(handler),
      audio_log_(std::move(audio_log)),
      reader_(std::move(reader)),
      foreign_socket_(std::move(foreign_socket)),
      stream_id_(stream_id),
      observer_(std::move(observer)),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(subscriber_);
  DCHECK(audio_manager);
  DCHECK(audio_log_);
  DCHECK(reader_);
  DCHECK(observer_);
  // Since the event handler never directly calls functions on |this| but rather
  // posts them to the IO thread, passing a pointer from the constructor is
  // safe.
  controller_event_handler_ = std::make_unique<ControllerEventHandler>(
      weak_factory_.GetWeakPtr(), stream_id_);
  controller_ = media::AudioOutputController::Create(
      audio_manager, controller_event_handler_.get(), params, output_device_id,
      AudioMirroringManager::ToGroupId(render_process_id, render_frame_id),
      reader_.get());
  DCHECK(controller_);
  if (media_observer)
    media_observer->OnCreatingAudioStream(render_process_id, render_frame_id);
}

AudioOutputDelegateImpl::~AudioOutputDelegateImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UpdatePlayingState(false);
  audio_log_->OnClosed();

  // Since the ownership of |controller_| is shared, we instead use its Close
  // method to stop callbacks from it. |controller_| will call the closure (on
  // the IO thread) when it's done closing, and it is only after that call that
  // we can delete |controller_event_handler_| and |reader_|. By giving the
  // closure ownership of these, we keep them alive until |controller_| is
  // closed.
  controller_->Close(base::BindOnce(
      [](std::unique_ptr<ControllerEventHandler> event_handler,
         std::unique_ptr<media::AudioSyncReader> reader,
         scoped_refptr<media::AudioOutputController> controller) {
        // Objects pointed to by the arguments are deleted on out-of-scope here.
      },
      std::move(controller_event_handler_), std::move(reader_), controller_));
}

int AudioOutputDelegateImpl::GetStreamId() {
  return stream_id_;
}

void AudioOutputDelegateImpl::OnPlayStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Play();
  audio_log_->OnStarted();
}

void AudioOutputDelegateImpl::OnPauseStream() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  controller_->Pause();
  audio_log_->OnStopped();
}

void AudioOutputDelegateImpl::OnSetVolume(double volume) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1);
  controller_->SetVolume(volume);
  audio_log_->OnSetVolume(volume);
}

void AudioOutputDelegateImpl::SendCreatedNotification() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  subscriber_->OnStreamCreated(stream_id_, reader_->TakeSharedMemoryRegion(),
                               std::move(foreign_socket_));
}

void AudioOutputDelegateImpl::UpdatePlayingState(bool playing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (playing == playing_)
    return;

  playing_ = playing;
  if (playing) {
    if (observer_)
      observer_->DidStartPlaying();
    if (media::AudioOutputController::will_monitor_audio_levels()) {
      DCHECK(!poll_timer_.IsRunning());
      // base::Unretained is safe in this case because |this| owns
      // |poll_timer_|.
      poll_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(1) / kPowerMeasurementsPerSecond,
          base::Bind(&AudioOutputDelegateImpl::PollAudioLevel,
                     base::Unretained(this)));
    } else if (observer_) {
      observer_->DidChangeAudibleState(true);
    }
  } else {
    if (media::AudioOutputController::will_monitor_audio_levels()) {
      DCHECK(poll_timer_.IsRunning());
      poll_timer_.Stop();
    }
    if (observer_)
      observer_->DidStopPlaying();
  }
}

void AudioOutputDelegateImpl::OnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  audio_log_->OnError();
  subscriber_->OnStreamError(stream_id_);
}

media::AudioOutputController* AudioOutputDelegateImpl::GetControllerForTesting()
    const {
  return controller_.get();
}

void AudioOutputDelegateImpl::PollAudioLevel() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool was_audible = is_audible_;
  is_audible_ = IsAudible();

  if (observer_ && is_audible_ != was_audible)
    observer_->DidChangeAudibleState(is_audible_);
}

bool AudioOutputDelegateImpl::IsAudible() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  float power_dbfs = controller_->ReadCurrentPowerAndClip().first;
  return power_dbfs >= kSilenceThresholdDBFS;
}

}  // namespace content

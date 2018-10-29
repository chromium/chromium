// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_audio_input_stream.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/audio/audio_input_controller.h"
#include "media/audio/audio_input_sync_writer.h"
#include "media/audio/audio_logging.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/user_input_monitor.h"

namespace content {

namespace {

void NotifyProcessHostStreamAdded(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process_host = RenderProcessHost::FromID(render_process_id);
  if (process_host)
    process_host->OnMediaStreamAdded();
}

void NotifyProcessHostStreamRemoved(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process_host = RenderProcessHost::FromID(render_process_id);
  if (process_host)
    process_host->OnMediaStreamRemoved();
}

// Safe to call from any thread.
void LogMessage(int stream_id, const std::string& message) {
  const std::string out_message =
      base::StringPrintf("[stream_id=%d] %s", stream_id, message.c_str());
  MediaStreamManager::SendMessageToNativeLog(out_message);
  DVLOG(1) << out_message;
}

}  // namespace

class AudioInputDelegateImpl::ControllerEventHandler
    : public media::AudioInputController::EventHandler {
 public:
  ControllerEventHandler(int stream_id,
                         base::WeakPtr<AudioInputDelegateImpl> weak_delegate)
      : stream_id_(stream_id), weak_delegate_(std::move(weak_delegate)) {}

  void OnCreated(bool initially_muted) override {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&AudioInputDelegateImpl::SendCreatedNotification,
                       weak_delegate_, initially_muted));
  }

  void OnError(media::AudioInputController::ErrorCode error_code) override {
    // To ensure that the error is logged even during the destruction sequence,
    // we log it here.
    LogMessage(stream_id_,
               base::StringPrintf("AIC reports error_code=%d", error_code));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&AudioInputDelegateImpl::OnError, weak_delegate_));
  }

  void OnLog(base::StringPiece message) override {
    LogMessage(stream_id_, message.as_string());
  }

  void OnMuted(bool is_muted) override {
    LogMessage(stream_id_, is_muted ? "OnMuted: State changed to muted"
                                    : "OnMuted: State changed to not muted");
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                             base::BindOnce(&AudioInputDelegateImpl::OnMuted,
                                            weak_delegate_, is_muted));
  }

 private:
  const int stream_id_;

  // Bound to the IO thread.
  const base::WeakPtr<AudioInputDelegateImpl> weak_delegate_;
};

std::unique_ptr<media::AudioInputDelegate> AudioInputDelegateImpl::Create(
    media::AudioManager* audio_manager,
    AudioMirroringManager* mirroring_manager,
    media::UserInputMonitor* user_input_monitor,
    int render_process_id,
    int render_frame_id,
    AudioInputDeviceManager* audio_input_device_manager,
    media::mojom::AudioLogPtr audio_log,
    AudioInputDeviceManager::KeyboardMicRegistration keyboard_mic_registration,
    uint32_t shared_memory_count,
    int stream_id,
    int session_id,
    bool automatic_gain_control,
    const media::AudioParameters& audio_parameters,
    EventHandler* subscriber) {
  // Check if we have the permission to open the device and which device to use.
  const MediaStreamDevice* device =
      audio_input_device_manager->GetOpenedDeviceById(session_id);
  if (!device) {
    LogMessage(stream_id, "Permission for stream not granted.");
    DLOG(WARNING) << "No permission has been granted to input stream with "
                  << "session_id=" << session_id;
    return nullptr;
  }

  media::AudioParameters possibly_modified_parameters = audio_parameters;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)) {
    possibly_modified_parameters.set_format(media::AudioParameters::AUDIO_FAKE);
  }

  auto foreign_socket = std::make_unique<base::CancelableSyncSocket>();

  std::unique_ptr<media::AudioInputSyncWriter> writer =
      media::AudioInputSyncWriter::Create(
          base::BindRepeating(&LogMessage, stream_id), shared_memory_count,
          possibly_modified_parameters, foreign_socket.get());

  if (!writer) {
    LogMessage(stream_id, "Failed to set up sync writer.");
    return nullptr;
  }

  LogMessage(
      stream_id,
      base::StringPrintf("OnCreateStream(render_frame_id=%d, session_id=%d): "
                         "device_name=%s, AGC=%d",
                         render_frame_id, session_id, device->name.c_str(),
                         automatic_gain_control));

  return base::WrapUnique(new AudioInputDelegateImpl(
      audio_manager, mirroring_manager, user_input_monitor,
      possibly_modified_parameters, render_process_id, std::move(audio_log),
      std::move(keyboard_mic_registration), stream_id, automatic_gain_control,
      subscriber, device, std::move(writer), std::move(foreign_socket)));
}

AudioInputDelegateImpl::AudioInputDelegateImpl(
    media::AudioManager* audio_manager,
    AudioMirroringManager* mirroring_manager,
    media::UserInputMonitor* user_input_monitor,
    const media::AudioParameters& audio_parameters,
    int render_process_id,
    media::mojom::AudioLogPtr audio_log,
    AudioInputDeviceManager::KeyboardMicRegistration keyboard_mic_registration,
    int stream_id,
    bool automatic_gain_control,
    EventHandler* subscriber,
    const MediaStreamDevice* device,
    std::unique_ptr<media::AudioInputSyncWriter> writer,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket)
    : subscriber_(subscriber),
      controller_event_handler_(),
      writer_(std::move(writer)),
      foreign_socket_(std::move(foreign_socket)),
      audio_log_(std::move(audio_log)),
      controller_(),
      keyboard_mic_registration_(std::move(keyboard_mic_registration)),
      stream_id_(stream_id),
      render_process_id_(render_process_id),
      weak_factory_(this) {
  // Prevent process backgrounding while audio input is active:
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyProcessHostStreamAdded, render_process_id_));

  controller_event_handler_ = std::make_unique<ControllerEventHandler>(
      stream_id_, weak_factory_.GetWeakPtr());

  const std::string& device_id = device->id;

  if (WebContentsMediaCaptureId::Parse(device_id, nullptr)) {
    // For MEDIA_GUM_DESKTOP_AUDIO_CAPTURE, the source is selected from
    // picker window, we do not mute the source audio. For
    // MEDIA_GUM_TAB_AUDIO_CAPTURE, the probable use case is Cast, we mute
    // the source audio.
    // TODO(qiangchen): Analyze audio constraints to make a duplicating or
    // diverting decision. It would give web developer more flexibility.
    controller_ = media::AudioInputController::CreateForStream(
        audio_manager->GetTaskRunner(), controller_event_handler_.get(),
        WebContentsAudioInputStream::Create(
            device_id, audio_parameters, audio_manager->GetWorkerTaskRunner(),
            mirroring_manager),
        writer_.get(), user_input_monitor);
    DCHECK(controller_);
    // Only count for captures from desktop media picker dialog.
    if (device->type == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE)
      IncrementDesktopCaptureCounter(TAB_AUDIO_CAPTURER_CREATED);
  } else {
    controller_ = media::AudioInputController::Create(
        audio_manager, controller_event_handler_.get(), writer_.get(),
        user_input_monitor, audio_parameters, device_id,
        automatic_gain_control);

    // Only count for captures from desktop media picker dialog and system loop
    // back audio.
    if (device->type == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE &&
        (media::AudioDeviceDescription::IsLoopbackDevice(device_id))) {
      IncrementDesktopCaptureCounter(SYSTEM_LOOPBACK_AUDIO_CAPTURER_CREATED);
    }
  }
  DCHECK(controller_);

  audio_log_->OnCreated(audio_parameters, device_id);
}

AudioInputDelegateImpl::~AudioInputDelegateImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_log_->OnClosed();
  LogMessage(stream_id_, "Closing stream");

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyProcessHostStreamRemoved, render_process_id_));

  // We pass |controller_event_handler_| and |writer_| in here to make sure they
  // stay alive until |controller_| has finished closing.
  controller_->Close(base::BindOnce(
      [](int stream_id, std::unique_ptr<ControllerEventHandler>,
         std::unique_ptr<media::AudioInputSyncWriter>) {
        LogMessage(stream_id, "Stream is now closed");
      },
      stream_id_, std::move(controller_event_handler_), std::move(writer_)));
}

int AudioInputDelegateImpl::GetStreamId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return stream_id_;
}

void AudioInputDelegateImpl::OnRecordStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogMessage(stream_id_, "OnRecordStream");
  controller_->Record();
  audio_log_->OnStarted();
}

void AudioInputDelegateImpl::OnSetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(volume, 0);
  DCHECK_LE(volume, 1);
  controller_->SetVolume(volume);
  audio_log_->OnSetVolume(volume);
}

void AudioInputDelegateImpl::OnSetOutputDeviceForAec(
    const std::string& raw_output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  controller_->SetOutputDeviceForAec(raw_output_device_id);
  audio_log_->OnLogMessage("SetOutputDeviceForAec");
}

void AudioInputDelegateImpl::SendCreatedNotification(bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(foreign_socket_);
  subscriber_->OnStreamCreated(stream_id_, writer_->TakeSharedMemoryRegion(),
                               std::move(foreign_socket_), initially_muted);
}

void AudioInputDelegateImpl::OnMuted(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  subscriber_->OnMuted(stream_id_, is_muted);
}

void AudioInputDelegateImpl::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  audio_log_->OnError();
  subscriber_->OnStreamError(stream_id_);
}

}  // namespace content

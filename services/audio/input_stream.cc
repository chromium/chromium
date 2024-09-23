// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/input_stream.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"
#include "media/base/user_input_monitor.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/audio/input_sync_writer.h"
#include "services/audio/user_input_monitor.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"

namespace audio {

namespace {
const int kMaxInputChannels = 3;

using InputStreamErrorCode = media::mojom::InputStreamErrorCode;
using DisconnectReason =
    media::mojom::AudioInputStreamObserver::DisconnectReason;

const char* ErrorCodeToString(InputController::ErrorCode error) {
  switch (error) {
    case (InputController::STREAM_CREATE_ERROR):
      return "STREAM_CREATE_ERROR";
    case (InputController::STREAM_OPEN_ERROR):
      return "STREAM_OPEN_ERROR";
    case (InputController::STREAM_ERROR):
      return "STREAM_ERROR";
    case (InputController::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR):
      return "STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR";
    case (InputController::STREAM_OPEN_DEVICE_IN_USE_ERROR):
      return "STREAM_OPEN_DEVICE_IN_USE_ERROR";
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return "UNKNOWN_ERROR";
}

std::string GetCtorLogString(const std::string& device_id,
                             const media::AudioParameters& params,
                             bool enable_agc) {
  std::string str = base::StringPrintf("Ctor(");
  base::StringAppendF(&str, "{device_id=%s}, ", device_id.c_str());
  base::StringAppendF(&str, "{params=[%s]}, ",
                      params.AsHumanReadableString().c_str());
  base::StringAppendF(&str, "{enable_agc=%d})", enable_agc);
  return str;
}

}  // namespace

InputStream::InputStream(
    CreatedCallback created_callback,
    DeleteCallback delete_callback,
    mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
    mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
    mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
    mojo::PendingRemote<media::mojom::AudioLog> log,
    media::AudioManager* audio_manager,
    media::AecdumpRecordingManager* aecdump_recording_manager,
    std::unique_ptr<UserInputMonitor> user_input_monitor,
    DeviceOutputListener* device_output_listener,
    media::mojom::AudioProcessingConfigPtr processing_config,
    const std::string& device_id,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool enable_agc)
    : id_(base::UnguessableToken::Create()),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)),
      observer_(std::move(observer)),
      log_(std::move(log)),
      created_callback_(std::move(created_callback)),
      delete_callback_(std::move(delete_callback)),
      foreign_socket_(),
      writer_(InputSyncWriter::Create(
          log_ ? base::BindRepeating(&media::mojom::AudioLog::OnLogMessage,
                                     base::Unretained(log_.get()))
               : base::DoNothing(),
          shared_memory_count,
          params,
          &foreign_socket_)),
      user_input_monitor_(std::move(user_input_monitor)) {
  DCHECK(audio_manager);
  DCHECK(receiver_.is_bound());
  DCHECK(client_);
  DCHECK(created_callback_);
  DCHECK(delete_callback_);
  DCHECK(params.IsValid());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("audio", "audio::InputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN2("audio", "InputStream", this, "device id",
                                    device_id, "params",
                                    params.AsHumanReadableString());
  SendLogMessage("%s", GetCtorLogString(device_id, params, enable_agc).c_str());

  // |this| owns these objects, so unretained is safe.
  base::RepeatingClosure error_handler =
      base::BindRepeating(&InputStream::OnStreamError, base::Unretained(this),
                          std::optional<DisconnectReason>());
  receiver_.set_disconnect_handler(error_handler);
  client_.set_disconnect_handler(error_handler);

  if (observer_)
    observer_.set_disconnect_handler(std::move(error_handler));

  if (log_)
    log_->OnCreated(params, device_id);

  // Only MONO, STEREO and STEREO_AND_KEYBOARD_MIC channel layouts are expected,
  // see AudioManagerBase::MakeAudioInputStream().
  if (params.channels() > kMaxInputChannels) {
    OnStreamPlatformError();
    return;
  }

  if (!writer_) {
    OnStreamPlatformError();
    return;
  }

  controller_ = InputController::Create(
      audio_manager, this, writer_.get(), user_input_monitor_.get(),
      device_output_listener, aecdump_recording_manager,
      std::move(processing_config), params, device_id, enable_agc);
}

InputStream::~InputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage("Dtor()");

  if (log_)
    log_->OnClosed();

  if (observer_) {
    observer_.ResetWithReason(
        static_cast<uint32_t>(DisconnectReason::kTerminatedByClient),
        std::string());
  }

  if (created_callback_) {
    // Didn't manage to create the stream. Call the callback anyways as mandated
    // by mojo.
    std::move(created_callback_).Run(nullptr, false, std::nullopt);
  }

  if (!controller_) {
    // Didn't initialize properly, nothing to clean up.
    return;
  }

  // TODO(crbug.com/40558532): remove InputController::Close() after
  // content/ streams are removed, destructor should suffice.
  controller_->Close();

  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "InputStream", this);
  TRACE_EVENT_NESTABLE_ASYNC_END0("audio", "audio::InputStream", this);
}

void InputStream::SetOutputDeviceForAec(const std::string& output_device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(controller_);
  controller_->SetOutputDeviceForAec(output_device_id);
  SendLogMessage("%s({output_device_id=%s})", __func__,
                 output_device_id.c_str());
}

void InputStream::Record() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(controller_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "Record", this);
  SendLogMessage("%s()", __func__);
  controller_->Record();
  if (observer_)
    observer_->DidStartRecording();
  if (log_)
    log_->OnStarted();
}

void InputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  DCHECK(controller_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "SetVolume", this, "volume",
                                      volume);

  if (volume < 0 || volume > 1) {
    receiver_.ReportBadMessage("Invalid volume");
    OnStreamPlatformError();
    return;
  }

  controller_->SetVolume(volume);
  if (log_)
    log_->OnSetVolume(volume);
}

void InputStream::OnCreated(bool initially_muted) {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1("audio", "Created", this,
                                      "initially muted", initially_muted);
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  SendLogMessage("%s({muted=%s})", __func__,
                 initially_muted ? "true" : "false");

  base::ReadOnlySharedMemoryRegion shared_memory_region =
      writer_->TakeSharedMemoryRegion();
  if (!shared_memory_region.IsValid()) {
    OnStreamPlatformError();
    return;
  }

  mojo::PlatformHandle socket_handle(foreign_socket_.Take());
  DCHECK(socket_handle.is_valid());

  std::move(created_callback_)
      .Run({std::in_place, std::move(shared_memory_region),
            std::move(socket_handle)},
           initially_muted, id_);
}

DisconnectReason InputErrorToDisconnectReason(InputController::ErrorCode code) {
  switch (code) {
    case InputController::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR:
      return DisconnectReason::kSystemPermissions;
    case InputController::STREAM_OPEN_DEVICE_IN_USE_ERROR:
      return DisconnectReason::kDeviceInUse;
    default:
      break;
  }
  return DisconnectReason::kPlatformError;
}

InputStreamErrorCode InputControllerErrorToStreamError(
    InputController::ErrorCode code) {
  switch (code) {
    case InputController::STREAM_OPEN_SYSTEM_PERMISSIONS_ERROR:
      return InputStreamErrorCode::kSystemPermissions;
    case InputController::STREAM_OPEN_DEVICE_IN_USE_ERROR:
      return InputStreamErrorCode::kDeviceInUse;
    default:
      break;
  }
  return InputStreamErrorCode::kUnknown;
}

void InputStream::OnError(InputController::ErrorCode error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "Error", this);

  client_->OnError(InputControllerErrorToStreamError(error_code));
  if (log_)
    log_->OnError();
  SendLogMessage("%s({error_code=%s})", __func__,
                 ErrorCodeToString(error_code));
  OnStreamError(InputErrorToDisconnectReason(error_code));
}

void InputStream::OnLog(std::string_view message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (log_)
    log_->OnLogMessage(std::string(message) + " [id=" + id_.ToString() + "]");
}

void InputStream::OnMuted(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  client_->OnMutedStateChanged(is_muted);
}

void InputStream::OnStreamPlatformError() {
  OnStreamError(DisconnectReason::kPlatformError);
}

void InputStream::OnStreamError(
    std::optional<DisconnectReason> reason_to_report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("audio", "OnStreamError", this);

  if (reason_to_report.has_value()) {
    if (observer_) {
      observer_.ResetWithReason(static_cast<uint32_t>(reason_to_report.value()),
                                std::string());
    }
    SendLogMessage("%s()", __func__);
  }

  // Defer callback so we're not destructed while in the constructor.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&InputStream::CallDeleter, weak_factory_.GetWeakPtr()));
  receiver_.reset();
}

void InputStream::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);

  std::move(delete_callback_).Run(this);
}

void InputStream::SendLogMessage(const char* format, ...) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owning_sequence_);
  if (!log_)
    return;
  va_list args;
  va_start(args, format);
  log_->OnLogMessage("audio::IS::" + base::StringPrintV(format, args) +
                     base::StringPrintf(" [id=%s]", id_.ToString().c_str()));
  va_end(args);
}

}  // namespace audio

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_output_device_thread_callback.h"
#include "media/base/limits.h"

namespace media {

AudioOutputDevice::AudioOutputDevice(
    std::unique_ptr<AudioOutputIPC> ipc,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const AudioSinkParameters& sink_params,
    base::TimeDelta authorization_timeout)
    : io_task_runner_(io_task_runner),
      callback_(nullptr),
      ipc_(std::move(ipc)),
      state_(IDLE),
      session_id_(sink_params.session_id),
      device_id_(sink_params.device_id),
      stopping_hack_(false),
      did_receive_auth_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
      output_params_(AudioParameters::UnavailableDeviceParams()),
      device_status_(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL),
      auth_timeout_(authorization_timeout) {
  DCHECK(ipc_);
  DCHECK(io_task_runner_);
}

void AudioOutputDevice::Initialize(const AudioParameters& params,
                                   RenderCallback* callback) {
  CHECK(params.IsValid());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputDevice::InitializeOnIOThread, this, params,
                     // The lifecycle of `callback` is controlled by the owner
                     // who is responsible for calling Stop before deallocating
                     // it. InitializeOnIOThread verifies this before using
                     // callback and we would not want to try to persist the
                     // object here as it would break the ownership model.
                     base::UnsafeDangling(callback)));
}

void AudioOutputDevice::InitializeOnIOThread(
    const AudioParameters& params,
    MayBeDangling<RenderCallback> callback) {
  DCHECK(!callback_) << "Calling Initialize() twice?";
  DCHECK(params.IsValid());
  DVLOG(1) << __func__ << ": " << params.AsHumanReadableString();
  audio_parameters_ = params;

  base::AutoLock auto_lock(audio_thread_lock_);
  // If Stop() has already been called, RenderCallback has already been
  // destroyed. So |callback| would be a dangling pointer.
  if (!stopping_hack_)
    callback_ = callback;
}

AudioOutputDevice::~AudioOutputDevice() {
  {
    // Abort any pending callbacks. Technically we don't need to acquire the
    // lock here since there should be no other calls outstanding, but because
    // we've used the GUARDED_BY compiler syntax, we'll get an error without it.
    base::AutoLock auto_lock(device_info_lock_);
    if (pending_device_info_cb_) {
      std::move(pending_device_info_cb_)
          .Run(OutputDeviceInfo(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL));
    }
  }

#if DCHECK_IS_ON()
  // Make sure we've stopped the stream properly before destructing |this|.
  DCHECK(audio_thread_lock_.Try());
  DCHECK_EQ(state_, IDLE);
  DCHECK(!audio_thread_);
  DCHECK(!audio_callback_);
  DCHECK(!stopping_hack_);
  audio_thread_lock_.Release();
#endif  // DCHECK_IS_ON()
}

void AudioOutputDevice::RequestDeviceAuthorization() {
  TRACE_EVENT0("audio", "AudioOutputDevice::RequestDeviceAuthorization");
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputDevice::RequestDeviceAuthorizationOnIOThread,
                     this));
}

void AudioOutputDevice::Start() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Start");
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputDevice::CreateStreamOnIOThread, this));
}

void AudioOutputDevice::Stop() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Stop");
  {
    base::ScopedAllowBaseSyncPrimitives allow;
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.reset();
    stopping_hack_ = true;
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputDevice::ShutDownOnIOThread, this));
}

void AudioOutputDevice::Play() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Play");
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputDevice::PlayOnIOThread, this));
}

void AudioOutputDevice::Pause() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Pause");
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputDevice::PauseOnIOThread, this));
}

void AudioOutputDevice::Flush() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Flush");
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputDevice::FlushOnIOThread, this));
}

bool AudioOutputDevice::SetVolume(double volume) {
  TRACE_EVENT1("audio", "AudioOutputDevice::Pause", "volume", volume);

  if (volume < 0 || volume > 1.0)
    return false;

  return io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioOutputDevice::SetVolumeOnIOThread, this, volume));
}

OutputDeviceInfo AudioOutputDevice::GetOutputDeviceInfo() {
  TRACE_EVENT0("audio", "AudioOutputDevice::GetOutputDeviceInfo");
  DCHECK(!io_task_runner_->BelongsToCurrentThread());
  did_receive_auth_.Wait();
  return GetOutputDeviceInfo_Signaled();
}

void AudioOutputDevice::GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) {
  {
    // Hold the lock while checking the signal and setting the pending callback
    // to avoid racing with authorization completion on the IO thread.
    base::AutoLock auto_lock(device_info_lock_);
    if (!did_receive_auth_.IsSignaled()) {
      DCHECK(!pending_device_info_cb_);
      pending_device_info_cb_ =
          base::BindPostTaskToCurrentDefault(std::move(info_cb));
      return;
    }
  }

  // Always post to avoid the caller being reentrant. Local testing shows even
  // on a powerful desktop, we haven't received device authorization by this
  // point when AOD construction and GetOutputDeviceInfoAsync() happen back to
  // back (which is the most common use case).
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(info_cb), GetOutputDeviceInfo_Signaled()));
}

bool AudioOutputDevice::IsOptimizedForHardwareParameters() {
  return true;
}

bool AudioOutputDevice::CurrentThreadIsRenderingThread() {
  // Since this function is supposed to be called on the rendering thread,
  // it's safe to access |audio_callback_| here. It will always be valid when
  // the rendering thread is running.
  return audio_callback_->CurrentThreadIsAudioDeviceThread();
}

void AudioOutputDevice::RequestDeviceAuthorizationOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, IDLE);

  state_ = AUTHORIZATION_REQUESTED;
  ipc_->RequestDeviceAuthorization(this, session_id_, device_id_);

  if (auth_timeout_.is_positive()) {
    // Create the timer on the thread it's used on. It's guaranteed to be
    // deleted on the same thread since users must call Stop() before deleting
    // AudioOutputDevice; see ShutDownOnIOThread().
    auth_timeout_action_ = std::make_unique<base::OneShotTimer>();
    auth_timeout_action_->Start(
        FROM_HERE, auth_timeout_,
        base::BindOnce(&AudioOutputDevice::OnDeviceAuthorized, this,
                       OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT, AudioParameters(),
                       std::string()));
  }
}

void AudioOutputDevice::CreateStreamOnIOThread() {
  TRACE_EVENT0("audio", "AudioOutputDevice::Create");
  DCHECK(io_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock auto_lock(audio_thread_lock_);
    if (!stopping_hack_)
      DCHECK(callback_) << "Initialize hasn't been called";
  }
#endif
  DCHECK_NE(state_, STREAM_CREATION_REQUESTED);

  if (!ipc_) {
    NotifyRenderCallbackOfError();
    return;
  }

  if (state_ == IDLE && !(did_receive_auth_.IsSignaled() && device_id_.empty()))
    RequestDeviceAuthorizationOnIOThread();

  ipc_->CreateStream(this, audio_parameters_);
  // By default, start playing right away.
  ipc_->PlayStream();
  state_ = STREAM_CREATION_REQUESTED;
}

void AudioOutputDevice::PlayOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (audio_callback_)
    audio_callback_->InitializePlayStartTime();

  if (ipc_)
    ipc_->PlayStream();
}

void AudioOutputDevice::PauseOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (ipc_)
    ipc_->PauseStream();
}

void AudioOutputDevice::FlushOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (ipc_)
    ipc_->FlushStream();
}

void AudioOutputDevice::ShutDownOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (ipc_)
    ipc_->CloseStream();

  state_ = IDLE;

  // Destoy the timer on the thread it's used on.
  auth_timeout_action_.reset();

  UMA_HISTOGRAM_ENUMERATION("Media.Audio.Render.StreamCallbackError2",
                            had_error_);
  had_error_ = kNoError;

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  //
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't rely on the main thread existing either.
  base::AutoLock auto_lock_(audio_thread_lock_);
  base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_thread_join;
  audio_thread_.reset();
  audio_callback_.reset();
  stopping_hack_ = false;
}

void AudioOutputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (ipc_)
    ipc_->SetVolume(volume);
}

void AudioOutputDevice::OnError() {
  TRACE_EVENT0("audio", "AudioOutputDevice::OnError");

  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ == IDLE)
    return;

  // Don't dereference the callback object if the audio thread
  // is stopped or stopping.  That could mean that the callback
  // object has been deleted.
  // TODO(tommi): Add an explicit contract for clearing the callback
  // object.  Possibly require calling Initialize again or provide
  // a callback object via Start() and clear it in Stop().
  NotifyRenderCallbackOfError();
}

void AudioOutputDevice::OnDeviceAuthorized(
    OutputDeviceStatus device_status,
    const AudioParameters& output_params,
    const std::string& matched_device_id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  auth_timeout_action_.reset();

  // Do nothing if late authorization is received after timeout.
  if (!ipc_)
    return;

  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Render.OutputDeviceAuthorizationTimedOut",
                        device_status == OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT);
  LOG_IF(WARNING, device_status == OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT)
      << "Output device authorization timed out";

  // It may happen that a second authorization is received as a result to a
  // call to Start() after Stop(). If the status for the second authorization
  // differs from the first, it will not be reflected in |device_status_|
  // to avoid a race.
  // This scenario is unlikely. If it occurs, the new value will be
  // different from OUTPUT_DEVICE_STATUS_OK, so the AudioOutputDevice
  // will enter the |ipc_| == nullptr state anyway, which is the safe thing to
  // do. This is preferable to holding a lock.
  if (!did_receive_auth_.IsSignaled()) {
    device_status_ = device_status;
    UMA_HISTOGRAM_ENUMERATION("Media.Audio.Render.OutputDeviceStatus",
                              device_status, OUTPUT_DEVICE_STATUS_MAX + 1);
  }

  if (device_status == OUTPUT_DEVICE_STATUS_OK) {
    TRACE_EVENT0("audio", "AudioOutputDevice authorized");

    if (!did_receive_auth_.IsSignaled()) {
      output_params_ = output_params;

      // It's possible to not have a matched device obtained via session id. It
      // means matching output device through |session_id_| failed and the
      // default device is used.
      DCHECK(AudioDeviceDescription::UseSessionIdToSelectDevice(session_id_,
                                                                device_id_) ||
             matched_device_id_.empty());
      matched_device_id_ = matched_device_id;

      DVLOG(1) << "AudioOutputDevice authorized, session_id: " << session_id_
               << ", device_id: " << device_id_
               << ", matched_device_id: " << matched_device_id_;

      OnAuthSignal();
    }
  } else {
    TRACE_EVENT1("audio", "AudioOutputDevice not authorized", "auth status",
                 device_status_);

    // Closing IPC forces a Signal(), so no clients are locked waiting
    // indefinitely after this method returns.
    ipc_->CloseStream();
    OnIPCClosed();

    NotifyRenderCallbackOfError();
  }
}

void AudioOutputDevice::OnStreamCreated(
    base::UnsafeSharedMemoryRegion shared_memory_region,
    base::SyncSocket::ScopedHandle socket_handle,
    bool playing_automatically) {
  TRACE_EVENT0("audio", "AudioOutputDevice::OnStreamCreated");

  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(shared_memory_region.IsValid());
#if BUILDFLAG(IS_WIN)
  DCHECK(socket_handle.IsValid());
#else
  DCHECK(socket_handle.is_valid());
#endif
  DCHECK_GT(shared_memory_region.GetSize(), 0u);

  if (state_ != STREAM_CREATION_REQUESTED)
    return;

  // We can receive OnStreamCreated() on the IO thread after the client has
  // called Stop() but before ShutDownOnIOThread() is processed. In such a
  // situation |callback_| might point to freed memory. Instead of starting
  // |audio_thread_| do nothing and wait for ShutDownOnIOThread() to get called.
  //
  // TODO(scherkus): The real fix is to have sane ownership semantics. The fact
  // that |callback_| (which should own and outlive this object!) can point to
  // freed memory is a mess. AudioRendererSink should be non-refcounted so that
  // owners (WebRtcAudioDeviceImpl, AudioRendererImpl, etc...) can Stop() and
  // delete as they see fit. AudioOutputDevice should internally use WeakPtr
  // to handle teardown and thread hopping. See http://crbug.com/151051 for
  // details.
  {
    base::AutoLock auto_lock(audio_thread_lock_);
    if (stopping_hack_)
      return;

    DCHECK(!audio_thread_);
    DCHECK(!audio_callback_);

    audio_callback_ = std::make_unique<AudioOutputDeviceThreadCallback>(
        audio_parameters_, std::move(shared_memory_region), callback_);
    if (playing_automatically)
      audio_callback_->InitializePlayStartTime();
    audio_thread_ = std::make_unique<AudioDeviceThread>(
        audio_callback_.get(), std::move(socket_handle), "AudioOutputDevice",
        base::ThreadType::kRealtimeAudio);
  }
}

void AudioOutputDevice::OnIPCClosed() {
  TRACE_EVENT0("audio", "AudioOutputDevice::OnIPCClosed");
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  ipc_.reset();
  state_ = IDLE;

  OnAuthSignal();
}

OutputDeviceInfo AudioOutputDevice::GetOutputDeviceInfo_Signaled() {
  DCHECK(did_receive_auth_.IsSignaled());
  OutputDeviceInfo info(AudioDeviceDescription::UseSessionIdToSelectDevice(
                            session_id_, device_id_)
                            ? matched_device_id_
                            : device_id_,
                        device_status_, output_params_);
  TRACE_EVENT1("audio", "AudioOutputDevice::GetOutputDeviceInfo_Signaled",
               "info", info.AsHumanReadableString());
  return info;
}

void AudioOutputDevice::OnAuthSignal() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // This lock is held while signaling to avoid any thread safety issues while
  // GetOutputDeviceInfoAsync() may be checking the signal and modifying the
  // |pending_device_info_cb_| on another thread.
  //
  // We might be able to get away with signaling outside of the lock, but this
  // requires more careful construction for anyone checking the signal and
  // using the result to set or get the pending callback value. The failure
  // mode is also more subtle, callbacks will be lost versus a thread hang which
  // is more easily detectable in the production population.
  base::AutoLock auto_lock(device_info_lock_);

  // Signal to unblock any blocked threads waiting for parameters.
  did_receive_auth_.Signal();

  // The callback is always posted by way base::BindPostTaskToCurrentDefault()
  // usage upon receipt, so this is safe to run under the lock.
  if (pending_device_info_cb_)
    std::move(pending_device_info_cb_).Run(GetOutputDeviceInfo_Signaled());
}

void AudioOutputDevice::NotifyRenderCallbackOfError() {
  TRACE_EVENT0("audio", "AudioOutputDevice::NotifyRenderCallbackOfError");
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  base::AutoLock auto_lock(audio_thread_lock_);
  // Avoid signaling error if Initialize() hasn't been called yet, or if
  // Stop() has already been called.
  if (callback_ && !stopping_hack_) {
    // Update |had_error_| for UMA stats.
    if (audio_callback_)
      had_error_ = kErrorDuringRendering;
    else
      had_error_ = kErrorDuringCreation;
    callback_->OnRenderError();
  }
}

}  // namespace media

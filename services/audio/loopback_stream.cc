// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_stream.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/sync_socket.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "base/types/zip.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace audio {

// static
constexpr double LoopbackStream::kMaxVolume;

LoopbackStream::LoopbackStream(
    CreatedCallback created_callback,
    BindingLostCallback binding_lost_callback,
    scoped_refptr<base::SequencedTaskRunner> loopback_task_runner,
    mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
    mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
    mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id)
    : binding_lost_callback_(std::move(binding_lost_callback)),
      receiver_(this, std::move(receiver)),
      client_(std::move(client)),
      observer_(std::move(observer)),
      loopback_signal_provider_(
          params,
          LoopbackGroupObserver::CreateMatchingGroupObserver(coordinator,
                                                             group_id)),
      loopback_signal_forwarder_(
          nullptr,
          base::OnTaskRunnerDeleter(loopback_task_runner)) {
  TRACE_EVENT1("audio", "LoopbackStream::LoopbackStream", "params",
               params.AsHumanReadableString());

  // Generate an error and shut down automatically whenever any of the mojo
  // bindings is closed.
  receiver_.set_disconnect_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));
  client_.set_disconnect_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));
  observer_.set_disconnect_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));

  // Construct the components of the AudioDataPipe, for delivering the data to
  // the consumer. If successful, create the LoopbackSignalForwarder too.
  base::CancelableSyncSocket foreign_socket;
  std::unique_ptr<InputSyncWriter> writer = InputSyncWriter::Create(
      base::BindRepeating(
          [](const std::string& message) { VLOG(1) << message; }),
      shared_memory_count, params, &foreign_socket);
  if (writer) {
    base::UnsafeSharedMemoryRegion shared_memory_region =
        writer->TakeSharedMemoryRegion();
    mojo::PlatformHandle socket_handle;
    if (shared_memory_region.IsValid()) {
      socket_handle = mojo::PlatformHandle(foreign_socket.Take());
      if (socket_handle.is_valid()) {
        std::move(created_callback)
            .Run({std::in_place, std::move(shared_memory_region),
                  std::move(socket_handle)});
        loopback_signal_forwarder_.reset(new LoopbackSignalForwarder(
            std::move(loopback_task_runner), params, std::move(writer),
            &loopback_signal_provider_));
        return;  // Success!
      }
    }
  }

  // If this point is reached, one or more AudioDataPipe components failed to
  // initialize. Report the error.
  std::move(created_callback).Run(nullptr);
  OnError();
}

LoopbackStream::~LoopbackStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("audio", "LoopbackStream::~LoopbackStream");

  if (loopback_signal_forwarder_) {
    // Since the LoopbackSignalProvider is about to be destroyed, we need to
    // prevent the LoopbackSignalForwarder from using it afterwards.
    loopback_signal_forwarder_->InvalidateLoopbackSignalProvider();
  }
}

void LoopbackStream::Record() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!loopback_signal_forwarder_ || loopback_signal_forwarder_->is_started()) {
    return;
  }

  TRACE_EVENT0("audio", "LoopbackStream::Record");

  // Start the data flow.
  loopback_signal_forwarder_->Start();

  if (observer_) {
    observer_->DidStartRecording();
  }
}

void LoopbackStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_INSTANT1("audio", "LoopbackStream::SetVolume",
                       TRACE_EVENT_SCOPE_THREAD, "volume", volume);

  if (!std::isfinite(volume) || volume < 0.0) {
    receiver_.ReportBadMessage("Invalid volume");
    OnError();
    return;
  }

  if (loopback_signal_forwarder_) {
    loopback_signal_forwarder_->SetVolume(std::min(volume, kMaxVolume));
  }
}

void LoopbackStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!binding_lost_callback_) {
    return;  // OnError() was already called.
  }

  TRACE_EVENT0("audio", "LoopbackStream::OnError");

  receiver_.reset();
  if (client_) {
    client_->OnError(media::mojom::InputStreamErrorCode::kUnknown);
    client_.reset();
  }
  observer_.reset();

  // Post a task to run the BindingLostCallback, since this method can be called
  // from the constructor.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<LoopbackStream> weak_self,
             BindingLostCallback callback) {
            if (auto* self = weak_self.get()) {
              std::move(callback).Run(self);
            }
          },
          weak_factory_.GetWeakPtr(), std::move(binding_lost_callback_)));

  // No need to shut down anything else, as the destructor will run soon and
  // take care of the rest.
}

LoopbackStream::LoopbackSignalForwarder::LoopbackSignalForwarder(
    scoped_refptr<base::SequencedTaskRunner> loopback_task_runner,
    const media::AudioParameters& output_params,
    std::unique_ptr<InputSyncWriter> writer,
    LoopbackSignalProvider* loopback_signal_provider)
    : clock_(base::DefaultTickClock::GetInstance()),
      loopback_task_runner_(loopback_task_runner),
      output_params_(output_params),
      writer_(std::move(writer)),
      mix_bus_(media::AudioBus::Create(output_params_)),
      loopback_signal_provider_(loopback_signal_provider) {}

void LoopbackStream::LoopbackSignalForwarder::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  volume_ = volume;
}

void LoopbackStream::LoopbackSignalForwarder::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);
  DCHECK(!is_started());
  {
    // Even though there is no risk of simultaneous access yet we need to take
    // the lock to satisfy the GUARDED_BY annotation.
    base::AutoLock scoped_lock(lock_);
    if (!loopback_signal_provider_) {
      return;
    }
    loopback_signal_provider_->Start();
  }

  timer_.emplace();
  // Note: GenerateMoreAudio() will schedule the timer.

  first_generate_time_ = clock_->NowTicks();
  frames_elapsed_ = 0;
  next_generate_time_ = first_generate_time_;

  loopback_task_runner_->PostTask(
      FROM_HERE,
      // Unretained is safe because the destructor will always be invoked from a
      // task that runs afterwards.
      base::BindOnce(&LoopbackSignalForwarder::GenerateMoreAudio,
                     base::Unretained(this)));
}

void LoopbackStream::LoopbackSignalForwarder::
    InvalidateLoopbackSignalProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);
  base::AutoLock scoped_lock(lock_);
  loopback_signal_provider_ = nullptr;
}

LoopbackStream::LoopbackSignalForwarder::~LoopbackSignalForwarder() {
  DCHECK(loopback_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(loopback_signal_provider_, nullptr);
}

void LoopbackStream::LoopbackSignalForwarder::GenerateMoreAudio() {
  DCHECK(loopback_task_runner_->RunsTasksInCurrentSequence());

  TRACE_EVENT_WITH_FLOW0("audio", "GenerateMoreAudio", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  double output_volume;
  base::TimeTicks delayed_capture_time;
  {
    base::AutoLock scoped_lock(lock_);
    if (!loopback_signal_provider_) {
      // Access to the provider has been removed because the loopback stream
      // is shutting down. The LoopbackSignalForwarder will be destroyed soon.
      return;
    }
    output_volume = volume_;
    delayed_capture_time = loopback_signal_provider_->PullLoopbackData(
        mix_bus_.get(), next_generate_time_, volume_);
  }

  // Insert the result into the AudioDataPipe.
  writer_->Write(mix_bus_.get(), output_volume, delayed_capture_time, {});

  // Determine when to generate more audio again. This is done by advancing
  // the frame count by one interval's worth, then computing the TimeTicks
  // corresponding to the new frame count. Also, check the clock to detect
  // when the user's machine is overloaded and the output needs to skip
  // forward one or more intervals.
  const int frames_per_buffer = mix_bus_->frames();
  frames_elapsed_ += frames_per_buffer;
  next_generate_time_ =
      first_generate_time_ +
      base::Microseconds(frames_elapsed_ * base::Time::kMicrosecondsPerSecond /
                         output_params_.sample_rate());
  const base::TimeTicks now = clock_->NowTicks();
  if (next_generate_time_ < now) {
    TRACE_EVENT_INSTANT1("audio", "GenerateMoreAudio Is Behind",
                         TRACE_EVENT_SCOPE_THREAD, "µsec_behind",
                         (now - next_generate_time_).InMicroseconds());
    // Audio generation has fallen behind. Skip-ahead the frame counter so
    // that audio generation will resume for the next buffer after the one
    // that should be generating right now. http://crbug.com/847487
    const int64_t target_frame_count =
        (now - first_generate_time_).InMicroseconds() *
        output_params_.sample_rate() / base::Time::kMicrosecondsPerSecond;
    frames_elapsed_ =
        (target_frame_count / frames_per_buffer + 1) * frames_per_buffer;
    next_generate_time_ =
        first_generate_time_ +
        base::Microseconds(frames_elapsed_ *
                           base::Time::kMicrosecondsPerSecond /
                           output_params_.sample_rate());
  }

  // Note: It's acceptable for `next_generate_time_` to be slightly before
  // `now` due to integer truncation behaviors in the math above. The timer
  // task started below will just run immediately and there will be no harmful
  // effects in the next GenerateMoreAudio() call. http://crbug.com/847487
  timer_->Start(FROM_HERE, next_generate_time_, this,
                &LoopbackSignalForwarder::GenerateMoreAudio,
                base::subtle::DelayPolicy::kPrecise);
}

}  // namespace audio

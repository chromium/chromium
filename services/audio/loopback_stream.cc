// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_stream.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/sync_socket.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace audio {

// static
constexpr double LoopbackStream::kMaxVolume;

// static
constexpr base::TimeDelta LoopbackStream::kCaptureDelay;

LoopbackStream::LoopbackStream(
    CreatedCallback created_callback,
    BindingLostCallback binding_lost_callback,
    scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
    media::mojom::AudioInputStreamRequest request,
    media::mojom::AudioInputStreamClientPtr client,
    media::mojom::AudioInputStreamObserverPtr observer,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    LoopbackCoordinator* coordinator,
    const base::UnguessableToken& group_id)
    : binding_lost_callback_(std::move(binding_lost_callback)),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      observer_(std::move(observer)),
      coordinator_(coordinator),
      group_id_(group_id),
      network_(nullptr, base::OnTaskRunnerDeleter(flow_task_runner)),
      weak_factory_(this) {
  DCHECK(coordinator_);

  TRACE_EVENT1("audio", "LoopbackStream::LoopbackStream", "params",
               params.AsHumanReadableString());

  // Generate an error and shut down automatically whenever any of the mojo
  // bindings is closed.
  binding_.set_connection_error_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));
  client_.set_connection_error_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));
  observer_.set_connection_error_handler(
      base::BindOnce(&LoopbackStream::OnError, base::Unretained(this)));

  // As of this writing, only machines older than about 10 years won't be able
  // to produce high-resolution timestamps. In order to avoid adding extra
  // complexity to the implementation, simply refuse to operate without that
  // basic level of hardware support.
  //
  // Construct the components of the AudioDataPipe, for delivering the data to
  // the consumer. If successful, create the FlowNetwork too.
  if (base::TimeTicks::IsHighResolution()) {
    base::CancelableSyncSocket foreign_socket;
    std::unique_ptr<InputSyncWriter> writer = InputSyncWriter::Create(
        base::BindRepeating(
            [](const std::string& message) { VLOG(1) << message; }),
        shared_memory_count, params, &foreign_socket);
    if (writer) {
      base::ReadOnlySharedMemoryRegion shared_memory_region =
          writer->TakeSharedMemoryRegion();
      mojo::ScopedHandle socket_handle;
      if (shared_memory_region.IsValid()) {
        socket_handle = mojo::WrapPlatformFile(foreign_socket.Release());
        if (socket_handle.is_valid()) {
          std::move(created_callback)
              .Run({base::in_place, std::move(shared_memory_region),
                    std::move(socket_handle)});
          network_.reset(new FlowNetwork(std::move(flow_task_runner), params,
                                         std::move(writer)));
          return;  // Success!
        }
      }
    }
  } else /* if (!base::TimeTicks::IsHighResolution()) */ {
    LOG(ERROR) << "Refusing to start loop-back because this machine cannot "
                  "provide high-resolution timestamps.";
  }

  // If this point is reached, either the TimeTicks clock is not high resolution
  // or one or more AudioDataPipe components failed to initialize. Report the
  // error.
  std::move(created_callback).Run(nullptr);
  OnError();
}

LoopbackStream::~LoopbackStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT0("audio", "LoopbackStream::~LoopbackStream");

  if (network_) {
    if (network_->is_started()) {
      coordinator_->RemoveObserver(group_id_, this);
      while (!snoopers_.empty()) {
        OnMemberLeftGroup(snoopers_.begin()->first);
      }
    }
    DCHECK(snoopers_.empty());
  }
}

void LoopbackStream::Record() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_ || network_->is_started()) {
    return;
  }

  TRACE_EVENT0("audio", "LoopbackStream::Record");

  // Begin snooping on all group members. This will set up the mixer network
  // and begin accumulating audio data in the Snoopers' buffers.
  DCHECK(snoopers_.empty());
  coordinator_->ForEachMemberInGroup(
      group_id_, base::BindRepeating(&LoopbackStream::OnMemberJoinedGroup,
                                     base::Unretained(this)));
  coordinator_->AddObserver(group_id_, this);

  // Start the data flow.
  network_->Start();

  if (observer_) {
    observer_->DidStartRecording();
  }
}

void LoopbackStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT_INSTANT1("audio", "LoopbackStream::SetVolume",
                       TRACE_EVENT_SCOPE_THREAD, "volume", volume);

  if (!std::isfinite(volume) || volume < 0.0) {
    mojo::ReportBadMessage("Invalid volume");
    OnError();
    return;
  }

  if (network_) {
    network_->SetVolume(std::min(volume, kMaxVolume));
  }
}

void LoopbackStream::OnMemberJoinedGroup(LoopbackGroupMember* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_) {
    return;
  }

  TRACE_EVENT1("audio", "LoopbackStream::OnMemberJoinedGroup", "member",
               member);

  const media::AudioParameters& input_params = member->GetAudioParameters();
  const auto emplace_result = snoopers_.emplace(
      std::piecewise_construct, std::forward_as_tuple(member),
      std::forward_as_tuple(input_params, network_->output_params()));
  DCHECK(emplace_result.second);  // There was no pre-existing map entry.
  SnooperNode* const snooper = &(emplace_result.first->second);
  member->StartSnooping(snooper, Snoopable::SnoopingMode::kDeferred);
  network_->AddInput(snooper);
}

void LoopbackStream::OnMemberLeftGroup(LoopbackGroupMember* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_) {
    return;
  }

  TRACE_EVENT1("audio", "LoopbackStream::OnMemberLeftGroup", "member", member);

  const auto snoop_it = snoopers_.find(member);
  DCHECK(snoop_it != snoopers_.end());
  SnooperNode* const snooper = &(snoop_it->second);
  member->StopSnooping(snooper, Snoopable::SnoopingMode::kDeferred);
  network_->RemoveInput(snooper);
  snoopers_.erase(snoop_it);
}

void LoopbackStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!binding_lost_callback_) {
    return;  // OnError() was already called.
  }

  TRACE_EVENT0("audio", "LoopbackStream::OnError");

  binding_.Close();
  if (client_) {
    client_->OnError();
    client_.reset();
  }
  observer_.reset();

  // Post a task to run the BindingLostCallback, since this method can be called
  // from the constructor.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
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

LoopbackStream::FlowNetwork::FlowNetwork(
    scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
    const media::AudioParameters& output_params,
    std::unique_ptr<InputSyncWriter> writer)
    : clock_(base::DefaultTickClock::GetInstance()),
      flow_task_runner_(flow_task_runner),
      output_params_(output_params),
      writer_(std::move(writer)),
      mix_bus_(media::AudioBus::Create(output_params_)) {}

void LoopbackStream::FlowNetwork::AddInput(SnooperNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  DCHECK(!base::ContainsValue(inputs_, node));
  inputs_.push_back(node);
}

void LoopbackStream::FlowNetwork::RemoveInput(SnooperNode* node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  const auto it = std::find(inputs_.begin(), inputs_.end(), node);
  DCHECK(it != inputs_.end());
  inputs_.erase(it);
}

void LoopbackStream::FlowNetwork::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  volume_ = volume;
}

void LoopbackStream::FlowNetwork::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);
  DCHECK(!is_started());

  timer_.emplace(clock_);
  timer_->SetTaskRunner(flow_task_runner_);
  // Note: GenerateMoreAudio() will schedule the timer.

  first_generate_time_ = clock_->NowTicks();
  frames_elapsed_ = 0;
  next_generate_time_ = first_generate_time_;

  flow_task_runner_->PostTask(
      FROM_HERE,
      // Unretained is safe because the destructor will always be invoked from a
      // task that runs afterwards.
      base::BindOnce(&FlowNetwork::GenerateMoreAudio, base::Unretained(this)));
}

LoopbackStream::FlowNetwork::~FlowNetwork() {
  DCHECK(flow_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(inputs_.empty());
}

void LoopbackStream::FlowNetwork::GenerateMoreAudio() {
  DCHECK(flow_task_runner_->RunsTasksInCurrentSequence());

  TRACE_EVENT_WITH_FLOW0("audio", "GenerateMoreAudio", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // Always generate audio from the recent past, to prevent buffer underruns
  // in the inputs.
  const base::TimeTicks delayed_capture_time =
      next_generate_time_ - kCaptureDelay;

  // Drive the audio flows from the SnooperNodes and produce the single result
  // stream. Hold the lock during this part of the process to prevent any of the
  // control methods from altering the configuration of the network.
  double output_volume;
  {
    base::AutoLock scoped_lock(lock_);
    output_volume = volume_;

    // Render the audio from each input, apply this stream's volume setting by
    // scaling the data, then mix it all together to form a single audio
    // signal. If there are no snoopers, just render silence.
    auto it = inputs_.begin();
    if (it == inputs_.end()) {
      mix_bus_->Zero();
    } else {
      // Render the first input's signal directly into |mix_bus_|.
      (*it)->Render(delayed_capture_time, mix_bus_.get());
      mix_bus_->Scale(volume_);

      // Render each successive input's signal into |transfer_bus_|, and then
      // mix it into |mix_bus_|.
      ++it;
      if (it != inputs_.end()) {
        if (!transfer_bus_) {
          transfer_bus_ = media::AudioBus::Create(output_params_);
        }
        do {
          (*it)->Render(delayed_capture_time, transfer_bus_.get());
          for (int ch = 0; ch < transfer_bus_->channels(); ++ch) {
            media::vector_math::FMAC(transfer_bus_->channel(ch), volume_,
                                     transfer_bus_->frames(),
                                     mix_bus_->channel(ch));
          }
          ++it;
        } while (it != inputs_.end());
      }
    }
  }

  // Insert the result into the AudioDataPipe.
  writer_->Write(mix_bus_.get(), output_volume, false, delayed_capture_time);

  // Determine when to generate more audio again. This is done by advancing the
  // frame count by one interval's worth, then computing the TimeTicks
  // corresponding to the new frame count. Also, check the clock to detect when
  // the user's machine is overloaded and the output needs to skip forward one
  // or more intervals.
  const int frames_per_buffer = mix_bus_->frames();
  frames_elapsed_ += frames_per_buffer;
  const base::TimeTicks now = clock_->NowTicks();
  const int64_t required_frames_elapsed =
      (now - first_generate_time_).InMicroseconds() *
      output_params_.sample_rate() / base::Time::kMicrosecondsPerSecond;
  if (frames_elapsed_ < required_frames_elapsed) {
    TRACE_EVENT_INSTANT1("audio", "GenerateMoreAudio Is Behind",
                         TRACE_EVENT_SCOPE_THREAD, "frames_behind",
                         (required_frames_elapsed - frames_elapsed_));
    // Audio generation has fallen behind. Skip-ahead the frame counter so that
    // audio generation will resume for the next buffer after the one that
    // should be generating right now. http://crbug.com/847487
    const int64_t required_buffers_elapsed =
        ((required_frames_elapsed + frames_per_buffer - 1) / frames_per_buffer);
    frames_elapsed_ = (required_buffers_elapsed + 1) * frames_per_buffer;
  }
  next_generate_time_ =
      first_generate_time_ +
      base::TimeDelta::FromMicroseconds(frames_elapsed_ *
                                        base::Time::kMicrosecondsPerSecond /
                                        output_params_.sample_rate());

  // Use the OneShotTimer to call this method again at the desired time.
  DCHECK_GE(next_generate_time_ - now, base::TimeDelta());
  timer_->Start(FROM_HERE, next_generate_time_ - now, this,
                &FlowNetwork::GenerateMoreAudio);
}

}  // namespace audio

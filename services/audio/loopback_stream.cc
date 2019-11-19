// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_stream.h"

#include <algorithm>
#include <cinttypes>
#include <string>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sync_socket.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"
#include "mojo/public/cpp/system/buffer.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace audio {

namespace {

// Start with a conservative, but reasonable capture delay that should work for
// most platforms (i.e., not needing an increase during a loopback session).
constexpr base::TimeDelta kInitialCaptureDelay =
    base::TimeDelta::FromMilliseconds(20);

}  // namespace

// static
constexpr double LoopbackStream::kMaxVolume;

LoopbackStream::LoopbackStream(
    CreatedCallback created_callback,
    BindingLostCallback binding_lost_callback,
    scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
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
      coordinator_(coordinator),
      group_id_(group_id),
      network_(nullptr, base::OnTaskRunnerDeleter(flow_task_runner)) {
  DCHECK(coordinator_);

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
  // the consumer. If successful, create the FlowNetwork too.
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

  // If this point is reached, one or more AudioDataPipe components failed to
  // initialize. Report the error.
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

  if (!base::TimeTicks::IsHighResolution()) {
    // As of this writing, only machines manufactured before 2008 won't be able
    // to produce high-resolution timestamps. Since the buffer management logic
    // (to mitigate overruns/underruns) depends on them to function correctly,
    // simply return early (i.e., never start snooping on the |member|).
    TRACE_EVENT_INSTANT0("audio",
                         "LoopbackStream::OnMemberJoinedGroup Rejected",
                         TRACE_EVENT_SCOPE_THREAD);
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
  member->StartSnooping(snooper);
  network_->AddInput(snooper);
}

void LoopbackStream::OnMemberLeftGroup(LoopbackGroupMember* member) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_) {
    return;
  }

  const auto snoop_it = snoopers_.find(member);
  if (snoop_it == snoopers_.end()) {
    // See comments about "high-resolution timestamps" in OnMemberJoinedGroup().
    return;
  }

  TRACE_EVENT1("audio", "LoopbackStream::OnMemberLeftGroup", "member", member);

  SnooperNode* const snooper = &(snoop_it->second);
  member->StopSnooping(snooper);
  network_->RemoveInput(snooper);
  snoopers_.erase(snoop_it);
}

void LoopbackStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!binding_lost_callback_) {
    return;  // OnError() was already called.
  }

  TRACE_EVENT0("audio", "LoopbackStream::OnError");

  receiver_.reset();
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

// static
std::atomic<int> LoopbackStream::FlowNetwork::instance_count_;

LoopbackStream::FlowNetwork::FlowNetwork(
    scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
    const media::AudioParameters& output_params,
    std::unique_ptr<InputSyncWriter> writer)
    : clock_(base::DefaultTickClock::GetInstance()),
      flow_task_runner_(flow_task_runner),
      output_params_(output_params),
      writer_(std::move(writer)),
      mix_bus_(media::AudioBus::Create(output_params_)) {
  ++instance_count_;
  magic_bytes_ = 0x600DC0DEu;
  HelpDiagnoseCauseOfLoopbackCrash("constructed");
}

void LoopbackStream::FlowNetwork::AddInput(SnooperNode* node) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  if (inputs_.empty()) {
    HelpDiagnoseCauseOfLoopbackCrash("adding first input");
  }
  DCHECK(!base::Contains(inputs_, node));
  inputs_.push_back(node);
}

void LoopbackStream::FlowNetwork::RemoveInput(SnooperNode* node) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  const auto it = std::find(inputs_.begin(), inputs_.end(), node);
  DCHECK(it != inputs_.end());
  inputs_.erase(it);
  if (inputs_.empty()) {
    HelpDiagnoseCauseOfLoopbackCrash("removed last input");
  }
}

void LoopbackStream::FlowNetwork::SetVolume(double volume) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);

  base::AutoLock scoped_lock(lock_);
  volume_ = volume;
}

void LoopbackStream::FlowNetwork::Start() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);
  DCHECK(!is_started());

  timer_.emplace(clock_);
  // Note: GenerateMoreAudio() will schedule the timer.

  HelpDiagnoseCauseOfLoopbackCrash("starting");

  first_generate_time_ = clock_->NowTicks();
  frames_elapsed_ = 0;
  next_generate_time_ = first_generate_time_;
  capture_delay_ = kInitialCaptureDelay;

  flow_task_runner_->PostTask(
      FROM_HERE,
      // Unretained is safe because the destructor will always be invoked from a
      // task that runs afterwards.
      base::BindOnce(&FlowNetwork::GenerateMoreAudio, base::Unretained(this)));
}

LoopbackStream::FlowNetwork::~FlowNetwork() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK(flow_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(inputs_.empty());

  HelpDiagnoseCauseOfLoopbackCrash("destructing");
  magic_bytes_ = 0xDEADBEEFu;
  --instance_count_;
}

void LoopbackStream::FlowNetwork::GenerateMoreAudio() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK(flow_task_runner_->RunsTasksInCurrentSequence());

  TRACE_EVENT_WITH_FLOW0("audio", "GenerateMoreAudio", this,
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // Drive the audio flows from the SnooperNodes and produce the single result
  // stream. Hold the lock during this part of the process to prevent any of the
  // control methods from altering the configuration of the network.
  double output_volume;
  base::TimeTicks delayed_capture_time;
  {
    base::AutoLock scoped_lock(lock_);
    output_volume = volume_;

    HelpDiagnoseCauseOfLoopbackCrash("generating");

    // Compute the reference time to use for audio rendering. Query each input
    // node and update |capture_delay_|, if necessary. This is used to always
    // generate audio from a "safe point" in the recent past, to prevent buffer
    // underruns in the inputs. http://crbug.com/934770
    delayed_capture_time = next_generate_time_ - capture_delay_;
    for (SnooperNode* node : inputs_) {
      const base::Optional<base::TimeTicks> suggestion =
          node->SuggestLatestRenderTime(mix_bus_->frames());
      if (suggestion.value_or(delayed_capture_time) < delayed_capture_time) {
        const base::TimeDelta increase = delayed_capture_time - (*suggestion);
        TRACE_EVENT_INSTANT2("audio", "GenerateMoreAudio Capture Delay Change",
                             TRACE_EVENT_SCOPE_THREAD, "old capture delay (µs)",
                             capture_delay_.InMicroseconds(), "change (µs)",
                             increase.InMicroseconds());
        delayed_capture_time = *suggestion;
        capture_delay_ += increase;
      }
    }
    TRACE_COUNTER_ID1("audio", "Loopback Capture Delay (µs)", this,
                      capture_delay_.InMicroseconds());

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
  next_generate_time_ =
      first_generate_time_ +
      base::TimeDelta::FromMicroseconds(frames_elapsed_ *
                                        base::Time::kMicrosecondsPerSecond /
                                        output_params_.sample_rate());
  const base::TimeTicks now = clock_->NowTicks();
  if (next_generate_time_ < now) {
    TRACE_EVENT_INSTANT1("audio", "GenerateMoreAudio Is Behind",
                         TRACE_EVENT_SCOPE_THREAD, u8"µsec_behind",
                         (now - next_generate_time_).InMicroseconds());
    // Audio generation has fallen behind. Skip-ahead the frame counter so that
    // audio generation will resume for the next buffer after the one that
    // should be generating right now. http://crbug.com/847487
    const int64_t target_frame_count =
        (now - first_generate_time_).InMicroseconds() *
        output_params_.sample_rate() / base::Time::kMicrosecondsPerSecond;
    frames_elapsed_ =
        (target_frame_count / frames_per_buffer + 1) * frames_per_buffer;
    next_generate_time_ =
        first_generate_time_ +
        base::TimeDelta::FromMicroseconds(frames_elapsed_ *
                                          base::Time::kMicrosecondsPerSecond /
                                          output_params_.sample_rate());
  }

  // Note: It's acceptable for |next_generate_time_| to be slightly before |now|
  // due to integer truncation behaviors in the math above. The timer task
  // started below will just run immediately and there will be no harmful
  // effects in the next GenerateMoreAudio() call. http://crbug.com/847487
  timer_->Start(FROM_HERE, next_generate_time_ - now, this,
                &FlowNetwork::GenerateMoreAudio);
}

void LoopbackStream::FlowNetwork::HelpDiagnoseCauseOfLoopbackCrash(
    const char* event) {
  static crash_reporter::CrashKeyString<512> crash_string(
      "audio-service-loopback");
  const auto ToAbbreviatedParamsString =
      [](const media::AudioParameters& params) {
        return base::StringPrintf(
            "F%d|L%d|R%d|FPB%d", static_cast<int>(params.format()),
            static_cast<int>(params.channel_layout()), params.sample_rate(),
            params.frames_per_buffer());
      };
  std::vector<std::string> input_formats;
  input_formats.reserve(inputs_.size());
  for (const SnooperNode* input : inputs_) {
    input_formats.push_back(ToAbbreviatedParamsString(input->input_params()));
  }
  crash_string.Set(base::StringPrintf(
      "num_instances=%d, event=%s, elapsed=%" PRId64 ", first_gen_ts=%" PRId64
      ", next_gen_ts=%" PRId64
      ", has_transfer_bus=%c, format=%s, volume=%f, has_timer=%c, inputs={%s}",
      instance_count_.load(), event, frames_elapsed_,
      (first_generate_time_ - base::TimeTicks()).InMicroseconds(),
      (next_generate_time_ - base::TimeTicks()).InMicroseconds(),
      transfer_bus_ ? 'Y' : 'N',
      ToAbbreviatedParamsString(output_params_).c_str(), volume_,
      timer_ ? 'Y' : 'N', base::JoinString(input_formats, ", ").c_str()));

  // If there are any crashes from this code, please record to crbug.com/888478.
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  CHECK(mix_bus_);
  CHECK_GT(mix_bus_->channels(), 0);
  CHECK_EQ(mix_bus_->channels(), output_params_.channels());
  CHECK_GT(mix_bus_->frames(), 0);
  CHECK_EQ(mix_bus_->frames(), output_params_.frames_per_buffer());
  for (int i = 0; i < mix_bus_->channels(); ++i) {
    float* const data = mix_bus_->channel(i);
    CHECK(data);
    memset(data, 0, mix_bus_->frames() * sizeof(data[0]));
  }
}

}  // namespace audio

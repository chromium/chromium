// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_STREAM_H_
#define SERVICES_AUDIO_LOOPBACK_STREAM_H_

#include <atomic>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/input_controller.h"
#include "services/audio/input_sync_writer.h"
#include "services/audio/loopback_coordinator.h"
#include "services/audio/loopback_group_member.h"
#include "services/audio/snooper_node.h"

namespace base {
class TickClock;
}  // namespace base

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

// An AudioInputStream that provides the result of looping-back and
// mixing-together all current and future audio output streams in the same
// group. The loopback re-mixes the audio, if necessary, so that the resulting
// data stream's format matches the required AudioParameters.
//
// This is organized in three main components: 1) The LoopbackStream itself acts
// as a "shell" that manages mojo bindings and creates/controls the other
// components. 2) One or more SnooperNodes that buffer input data for each
// source OutputStream and format-convert it. 3) A "flow network" that runs via
// a different task runner, to take all the audio collected in the SnooperNodes
// and mix it into a single data stream.
class LoopbackStream : public media::mojom::AudioInputStream,
                       public LoopbackCoordinator::Observer {
 public:
  using CreatedCallback =
      base::OnceCallback<void(media::mojom::ReadOnlyAudioDataPipePtr)>;
  using BindingLostCallback = base::OnceCallback<void(LoopbackStream*)>;

  LoopbackStream(
      CreatedCallback created_callback,
      BindingLostCallback binding_lost_callback,
      scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& group_id);

  ~LoopbackStream() final;

  bool is_recording() const { return network_ && network_->is_started(); }

  // media::mojom::AudioInputStream implementation.
  void Record() final;
  void SetVolume(double volume) final;

  // LoopbackCoordinator::Observer implementation. When a member joins
  // a group, a SnooperNode is created for it, and a loopback flow from
  // LoopbackGroupMember → SnooperNode → FlowNetwork is built-up.
  void OnMemberJoinedGroup(LoopbackGroupMember* member) final;
  void OnMemberLeftGroup(LoopbackGroupMember* member) final;

  // Overrides for unit testing. These must be called before Record().
  void set_clock_for_testing(const base::TickClock* clock) {
    network_->set_clock_for_testing(clock);
  }
  void set_sync_writer_for_testing(
      std::unique_ptr<InputController::SyncWriter> writer) {
    network_->set_writer_for_testing(std::move(writer));
  }

  // Generally, a volume of 1.0 should be the maximum possible. However, there
  // are cases where requests to amplify are made by specifying values higher
  // than 1.0.
  static constexpr double kMaxVolume = 2.0;

 private:
  // Drives all audio flows, re-mixing the audio from multiple SnooperNodes into
  // a single audio stream. This class mainly operates on a separate task runner
  // from LoopbackStream and can only be destroyed by scheduling it to occur on
  // that same task runner.
  class FlowNetwork {
   public:
    FlowNetwork(scoped_refptr<base::SequencedTaskRunner> flow_task_runner,
                const media::AudioParameters& output_params,
                std::unique_ptr<InputSyncWriter> writer);

    // These must be called to override the Clock/SyncWriter before Start().
    void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }
    void set_writer_for_testing(
        std::unique_ptr<InputController::SyncWriter> writer) {
      writer_ = std::move(writer);
    }

    bool is_started() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(control_sequence_);
      return !!timer_;
    }

    const media::AudioParameters& output_params() const {
      return output_params_;
    }

    // Add/Remove an input into this flow network. These may be called at any
    // time, before or after Start(). All inputs must be removed before the
    // FlowNetwork is scheduled for destruction.
    void AddInput(SnooperNode* node);
    void RemoveInput(SnooperNode* node);

    // This may be called at any time, before or after Start(), to change the
    // volume setting.
    void SetVolume(double volume);

    // Start generating audio data. This must only be called once, and there is
    // no "stop" until destruction time.
    void Start();

   private:
    // Since this class guarantees its destructor will be called via the flow
    // task runner, and destruction is carried out only by base::DeleteHelper,
    // make the destructor is private.
    friend class base::DeleteHelper<FlowNetwork>;
    ~FlowNetwork();

    // Called periodically via the audio flow task runner to drive all the audio
    // flows from the SnooperNodes, mix them together, and output to the
    // AudioDataPipe. Each call schedules the next one until the |run_state_|
    // becomes stopped.
    void GenerateMoreAudio();

    // TODO(crbug.com/888478): Remove this and all call points after diagnosis.
    // This generates crash key strings exposing the current state of the flow
    // network, and also ensures |mix_bus_| is valid, hasn't been corrupted, and
    // that writing to its data arrays will not cause a page fault.
    void HelpDiagnoseCauseOfLoopbackCrash(const char* event);

    const base::TickClock* clock_;

    // Task runner that calls GenerateMoreAudio() to drive all the audio data
    // flows.
    const scoped_refptr<base::SequencedTaskRunner> flow_task_runner_;

    // The audio parameters of the output.
    const media::AudioParameters output_params_;

    // Destination for the output of this FlowNetwork.
    std::unique_ptr<InputController::SyncWriter> writer_;

    // Ensures thread-safe access to changing the |inputs_| and |volume_| while
    // running.
    base::Lock lock_;

    // The input nodes.
    std::vector<SnooperNode*> inputs_;  // Guarded by |lock_|.

    // Current stream volume. The audio output from this FlowNetwork is scaled
    // by this amount during mixing.
    double volume_ = 1.0;  // Guarded by |lock_|.

    // This is set once Start() is called, and lives until this FlowNetwork is
    // destroyed. It is used to schedule cancelable tasks run by the
    // |flow_task_runner_|.
    base::Optional<base::OneShotTimer> timer_;

    // These are used to compute when the |timer_| fires and calls
    // GenerateMoreAudio(). They ensure that each timer task is scheduled to
    // fire with a delay that accounted for how much time was spent processing.
    base::TimeTicks first_generate_time_;
    int64_t frames_elapsed_ = 0;
    base::TimeTicks next_generate_time_;

    // The amount of time in the past from which to capture the audio. The audio
    // recorded from each SnooperNode input is being generated with a target
    // playout time in the near future (usually 1 to 20 ms). To avoid underflow,
    // audio is always fetched from a safe position in the recent past.
    //
    // This is updated to match the SnooperNode whose recording is most delayed.
    base::TimeDelta capture_delay_;

    // Used to transfer the audio from each SnooperNode and mix them into a
    // single audio signal. |transfer_bus_| is only allocated when first needed,
    // but |mix_bus_| is allocated in the constructor because it is always
    // needed.
    std::unique_ptr<media::AudioBus> transfer_bus_;
    const std::unique_ptr<media::AudioBus> mix_bus_;

    // TODO(crbug.com/888478): Remove these after diagnosis.
    volatile uint32_t magic_bytes_;
    static std::atomic<int> instance_count_;

    SEQUENCE_CHECKER(control_sequence_);

    DISALLOW_COPY_AND_ASSIGN(FlowNetwork);
  };

  // Reports a fatal error to the client, and then runs the BindingLostCallback.
  void OnError();

  // Run when any of |receiver_|, |client_|, or |observer_| are closed. This
  // callback is generally used to automatically terminate this LoopbackStream.
  BindingLostCallback binding_lost_callback_;

  // Mojo bindings. If any of these is closed, the LoopbackStream will call
  // OnError(), which will run the |binding_lost_callback_|.
  mojo::Receiver<media::mojom::AudioInputStream> receiver_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  mojo::Remote<media::mojom::AudioInputStreamObserver> observer_;

  // Used for identifying group members and snooping on their audio data flow.
  LoopbackCoordinator* const coordinator_;
  const base::UnguessableToken group_id_;

  // The snoopers associated with each group member. This is not a flat_map
  // because SnooperNodes cannot move around in memory while in operation.
  std::map<LoopbackGroupMember*, SnooperNode> snoopers_;

  // The flow network that generates the single loopback result stream. It is
  // owned by LoopbackStream, but it's destruction must be carried out by the
  // flow task runner. This is never null, unless the system cannot support
  // loopback (see constructor definition comments).
  std::unique_ptr<FlowNetwork, base::OnTaskRunnerDeleter> network_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LoopbackStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LoopbackStream);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_STREAM_H_

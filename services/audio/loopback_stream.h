// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_STREAM_H_
#define SERVICES_AUDIO_LOOPBACK_STREAM_H_

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
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
#include "services/audio/loopback_signal_provider.h"

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
// components. 2) A LoopbackSignalProvider which provides a mix of audio from a
// group of output streams. 3) A LoopbackSignalForwarder that runs via a
// different task runner, which pulls audio from the Provider and pushes it to
// the InputSyncWriter.
class LoopbackStream final : public media::mojom::AudioInputStream {
 public:
  using CreatedCallback =
      base::OnceCallback<void(media::mojom::ReadWriteAudioDataPipePtr)>;
  using BindingLostCallback = base::OnceCallback<void(LoopbackStream*)>;

  LoopbackStream(
      CreatedCallback created_callback,
      BindingLostCallback binding_lost_callback,
      scoped_refptr<base::SequencedTaskRunner> loopback_task_runner,
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      LoopbackCoordinator* coordinator,
      const base::UnguessableToken& group_id);

  LoopbackStream(const LoopbackStream&) = delete;
  LoopbackStream& operator=(const LoopbackStream&) = delete;

  ~LoopbackStream() final;

  bool is_recording() const {
    return loopback_signal_forwarder_ &&
           loopback_signal_forwarder_->is_started();
  }

  // media::mojom::AudioInputStream implementation.
  void Record() final;
  void SetVolume(double volume) final;

  // Overrides for unit testing. These must be called before Record().
  void set_clock_for_testing(const base::TickClock* clock) {
    loopback_signal_forwarder_->set_clock_for_testing(clock);
  }
  void set_sync_writer_for_testing(
      std::unique_ptr<InputController::SyncWriter> writer) {
    loopback_signal_forwarder_->set_writer_for_testing(std::move(writer));
  }

  // Generally, a volume of 1.0 should be the maximum possible. However, there
  // are cases where requests to amplify are made by specifying values higher
  // than 1.0.
  static constexpr double kMaxVolume = 2.0;

 private:
  void AddLoopbackSource(LoopbackSource* source);
  void RemoveLoopbackSource(LoopbackSource* source);

  // Drives all audio flows, pulling audio from the LoopbackSignalProvider and
  // pushing it into the InputSyncWriter. This class mainly operates on a
  // separate task runner from LoopbackStream and can only be destroyed by
  // scheduling it to occur on that same task runner.
  class LoopbackSignalForwarder {
   public:
    LoopbackSignalForwarder(
        scoped_refptr<base::SequencedTaskRunner> loopback_task_runner,
        const media::AudioParameters& output_params,
        std::unique_ptr<InputSyncWriter> writer,
        LoopbackSignalProvider* loopback_signal_provider);

    LoopbackSignalForwarder(const LoopbackSignalForwarder&) = delete;
    LoopbackSignalForwarder& operator=(const LoopbackSignalForwarder&) = delete;

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

    // This may be called at any time, before or after Start(), to change the
    // volume setting.
    void SetVolume(double volume);

    // Start generating audio data. This must only be called once, and there is
    // no "stop" until destruction time.
    void Start();

    // Called by the owning LoopbackStream to clear the
    // `loopback_signal_provider_` pointer before destruction.
    void InvalidateLoopbackSignalProvider();

   private:
    // Since this class guarantees its destructor will be called via the
    // loopback task runner, and destruction is carried out only by
    // base::DeleteHelper, make the destructor is private.
    friend class base::DeleteHelper<LoopbackSignalForwarder>;
    ~LoopbackSignalForwarder();

    // Called periodically via the audio loopback task runner pull audio from
    // the LoopbackSignalProvider and push to the InputSyncWriter. Each call
    // schedules the next one until the `run_state_` becomes stopped.
    void GenerateMoreAudio();

    raw_ptr<const base::TickClock> clock_;

    // Task runner that calls GenerateMoreAudio() to drive all the audio data
    // flows.
    const scoped_refptr<base::SequencedTaskRunner> loopback_task_runner_;

    // The audio parameters of the output.
    const media::AudioParameters output_params_;

    // Destination for the output of this LoopbackSignalForwarder.
    std::unique_ptr<InputController::SyncWriter> writer_;

    // Ensures thread-safe access to `volume_` and `loopback_signal_provider_`,
    // which are accessed on both the main thread and the audio thread.
    base::Lock lock_;

    // Current stream volume. The audio output from this LoopbackSignalForwarder
    // is scaled by this amount during mixing.
    double volume_ GUARDED_BY(lock_) = 1.0;

    // This is set once Start() is called, and lives until this
    // LoopbackSignalForwarder is destroyed. It is used to schedule cancelable
    // tasks run by the `loopback_task_runner_`.
    std::optional<base::DeadlineTimer> timer_;

    // These are used to compute when the `timer_` fires and calls
    // GenerateMoreAudio(). They ensure that each timer task is scheduled to
    // fire with a delay that accounted for how much time was spent processing.
    base::TimeTicks first_generate_time_;
    int64_t frames_elapsed_ = 0;
    base::TimeTicks next_generate_time_;

    // Used to as destination for the audio pulled from
    // `loopback_signal_provider_`.
    const std::unique_ptr<media::AudioBus> mix_bus_;

    // Used to pull audio from the output streams.
    raw_ptr<LoopbackSignalProvider> loopback_signal_provider_ GUARDED_BY(lock_);

    SEQUENCE_CHECKER(control_sequence_);
  };

  // Reports a fatal error to the client, and then runs the BindingLostCallback.
  void OnError();

  // Run when any of `receiver_`, `client_`, or `observer_` are closed. This
  // callback is generally used to automatically terminate this LoopbackStream.
  BindingLostCallback binding_lost_callback_;

  // Mojo bindings. If any of these is closed, the LoopbackStream will call
  // OnError(), which will run the `binding_lost_callback_`.
  mojo::Receiver<media::mojom::AudioInputStream> receiver_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  mojo::Remote<media::mojom::AudioInputStreamObserver> observer_;

  // Used to pull loopback audio from the target output streams.
  LoopbackSignalProvider loopback_signal_provider_;

  // The forwarder that generates the single loopback result stream. It is
  // owned by LoopbackStream, but it's destruction must be carried out by the
  // loopback task runner. This is never null, unless the system cannot support
  // loopback (see constructor definition comments).
  std::unique_ptr<LoopbackSignalForwarder, base::OnTaskRunnerDeleter>
      loopback_signal_forwarder_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<LoopbackStream> weak_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_STREAM_H_

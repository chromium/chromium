// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/iir_filter_handler.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel_processor.h"
#include "third_party/blink/renderer/platform/audio/iir_filter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

constexpr uint32_t kNumberOfChannels = 1;

constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

class IIRProcessor final : public AudioDSPKernelProcessor {
 public:
  IIRProcessor(float sample_rate,
               uint32_t number_of_channels,
               unsigned render_quantum_frames,
               const Vector<double>& feedforward_coef,
               const Vector<double>& feedback_coef,
               bool is_filter_stable)
      : AudioDSPKernelProcessor(sample_rate,
                                number_of_channels,
                                render_quantum_frames),
        is_filter_stable_(is_filter_stable) {
    unsigned feedback_length = feedback_coef.size();
    unsigned feedforward_length = feedforward_coef.size();
    DCHECK_GT(feedback_length, 0u);
    DCHECK_GT(feedforward_length, 0u);

    feedforward_.Allocate(feedforward_length);
    feedback_.Allocate(feedback_length);
    feedforward_.CopyToRange(feedforward_coef.data(), 0, feedforward_length);
    feedback_.CopyToRange(feedback_coef.data(), 0, feedback_length);

    // Need to scale the feedback and feedforward coefficients appropriately.
    // (It's up to the caller to ensure feedbackCoef[0] is not 0.)
    DCHECK_NE(feedback_coef[0], 0);

    if (feedback_coef[0] != 1) {
      // The provided filter is:
      //
      //   a[0]*y(n) + a[1]*y(n-1) + ... = b[0]*x(n) + b[1]*x(n-1) + ...
      //
      // We want the leading coefficient of y(n) to be 1:
      //
      //   y(n) + a[1]/a[0]*y(n-1) + ... = b[0]/a[0]*x(n) + b[1]/a[0]*x(n-1) +
      //   ...
      //
      // Thus, the feedback and feedforward coefficients need to be scaled by
      // 1/a[0].
      float scale = feedback_coef[0];
      for (unsigned k = 1; k < feedback_length; ++k) {
        feedback_[k] /= scale;
      }

      for (unsigned k = 0; k < feedforward_length; ++k) {
        feedforward_[k] /= scale;
      }

      // The IIRFilter checks to make sure this coefficient is 1, so make it so.
      feedback_[0] = 1;
    }

    response_kernel_ = std::make_unique<IIRDSPKernel>(this);
  }

  ~IIRProcessor() override {
    if (IsInitialized()) {
      Uninitialize();
    }
  }

  std::unique_ptr<AudioDSPKernel> CreateKernel() override {
    return std::make_unique<IIRDSPKernel>(this);
  }

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response) {
    response_kernel_->GetFrequencyResponse(n_frequencies, frequency_hz,
                                           mag_response, phase_response);
  }

  AudioDoubleArray* Feedback() { return &feedback_; }
  AudioDoubleArray* Feedforward() { return &feedforward_; }
  bool IsFilterStable() const { return is_filter_stable_; }

 private:
  class IIRDSPKernel final : public AudioDSPKernel {
   public:
    explicit IIRDSPKernel(IIRProcessor* processor)
        : AudioDSPKernel(processor),
          iir_(processor->Feedforward(), processor->Feedback()) {
      tail_time_ =
          iir_.TailTime(processor->SampleRate(), processor->IsFilterStable(),
                        processor->RenderQuantumFrames());
    }

    // AudioDSPKernel
    void Process(const float* source,
                 float* dest,
                 uint32_t frames_to_process) override {
      DCHECK(source);
      DCHECK(dest);

      iir_.Process(source, dest, frames_to_process);
    }
    void Reset() override { iir_.Reset(); }

    // Get the magnitude and phase response of the filter at the given
    // set of frequencies (in Hz). The phase response is in radians.
    void GetFrequencyResponse(int n_frequencies,
                              const float* frequency_hz,
                              float* mag_response,
                              float* phase_response) {
      DCHECK_GE(n_frequencies, 0);
      DCHECK(frequency_hz);
      DCHECK(mag_response);
      DCHECK(phase_response);

      Vector<float> frequency(n_frequencies);

      double nyquist = Nyquist();

      // Convert from frequency in Hz to normalized frequency (0 -> 1),
      // with 1 equal to the Nyquist frequency.
      for (int k = 0; k < n_frequencies; ++k) {
        frequency[k] = frequency_hz[k] / nyquist;
      }

      iir_.GetFrequencyResponse(n_frequencies, frequency.data(), mag_response,
                                phase_response);
    }

    double TailTime() const override { return tail_time_; }
    double LatencyTime() const override { return 0; }
    bool RequiresTailProcessing() const override {
      // Always return true even if the tail time and latency might both be
      // zero.
      return true;
    }

   private:
    IIRFilter iir_;
    double tail_time_;
  };

  // The feedback and feedforward filter coefficients for the IIR filter.
  AudioDoubleArray feedback_;
  AudioDoubleArray feedforward_;
  bool is_filter_stable_;

  // This holds the IIR kernel for computing the frequency response.
  std::unique_ptr<IIRDSPKernel> response_kernel_;
};

IIRFilterHandler::IIRFilterHandler(AudioNode& node,
                                   float sample_rate,
                                   const Vector<double>& feedforward_coef,
                                   const Vector<double>& feedback_coef,
                                   bool is_filter_stable)
    : AudioHandler(kNodeTypeIIRFilter, node, sample_rate),
      processor_(std::make_unique<IIRProcessor>(
          sample_rate,
          kNumberOfChannels,
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
          feedforward_coef,
          feedback_coef,
          is_filter_stable)) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);
  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);
}

scoped_refptr<IIRFilterHandler> IIRFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    const Vector<double>& feedforward_coef,
    const Vector<double>& feedback_coef,
    bool is_filter_stable) {
  return base::AdoptRef(new IIRFilterHandler(
      node, sample_rate, feedforward_coef, feedback_coef, is_filter_stable));
}

IIRFilterHandler::~IIRFilterHandler() {
  // Safe to call the uninitialize() because it's final.
  Uninitialize();
}
void IIRFilterHandler::Process(uint32_t frames_to_process) {
  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized() || !Processor() ||
      Processor()->NumberOfChannels() != NumberOfChannels()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    // FIXME: if we take "tail time" into account, then we can avoid calling
    // processor()->process() once the tail dies down.
    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    Processor()->Process(source_bus.get(), destination_bus, frames_to_process);
  }

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(*task_runner_, FROM_HERE,
                          CrossThreadBindOnce(&IIRFilterHandler::NotifyBadState,
                                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void IIRFilterHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized() || !Processor()) {
    return;
  }

  Processor()->ProcessOnlyAudioParams(frames_to_process);
}

// Nice optimization in the very common case allowing for "in-place" processing
void IIRFilterHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - suggest to the input to render directly into output
  // bus for in-place processing in process() if possible.
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

void IIRFilterHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Initialize();

  AudioHandler::Initialize();
}

void IIRFilterHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Uninitialize();

  AudioHandler::Uninitialize();
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void IIRFilterHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &Input(0));
  DCHECK(Processor());

  unsigned number_of_channels = input->NumberOfChannels();

  if (IsInitialized() && number_of_channels != Output(0).NumberOfChannels()) {
    // We're already initialized but the channel count has changed.
    Uninitialize();
  }

  if (!IsInitialized()) {
    // This will propagate the channel count to any nodes connected further down
    // the chain...
    Output(0).SetNumberOfChannels(number_of_channels);

    // Re-initialize the processor with the new channel count.
    Processor()->SetNumberOfChannels(number_of_channels);
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

unsigned IIRFilterHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

void IIRFilterHandler::GetFrequencyResponse(int n_frequencies,
                                            const float* frequency_hz,
                                            float* mag_response,
                                            float* phase_response) {
  static_cast<IIRProcessor*>(Processor())
      ->GetFrequencyResponse(n_frequencies, frequency_hz, mag_response,
                             phase_response);
}

bool IIRFilterHandler::HasNonFiniteOutput() const {
  AudioBus* output_bus = Output(0).Bus();

  for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
    AudioChannel* channel = output_bus->Channel(k);
    if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
      return true;
    }
  }

  return false;
}

void IIRFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          NodeTypeName() + ": state is bad, probably due to unstable filter."));
}

bool IIRFilterHandler::RequiresTailProcessing() const {
  return processor_->RequiresTailProcessing();
}

double IIRFilterHandler::TailTime() const {
  return processor_->TailTime();
}

double IIRFilterHandler::LatencyTime() const {
  return processor_->LatencyTime();
}

}  // namespace blink

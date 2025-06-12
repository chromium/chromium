// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/biquad_filter_handler.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/biquad.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

constexpr unsigned kRenderQuantumFramesExpected = 128;
constexpr uint32_t kNumberOfChannels = 1;
constexpr unsigned kDefaultNumberOfOutputChannels = 1;

bool HasConstantValues(float* values, int frames_to_process) {
  // Load the initial value
  const float value = values[0];
  // This initialization ensures that we correctly handle the first frame and
  // start the processing from the second frame onwards, effectively excluding
  // the first frame from the subsequent comparisons in the non-SIMD paths
  // it guarantees that we don't redundantly compare the first frame again
  // during the loop execution.
  int processed_frames = 1;

#if defined(__SSE2__)
  // Process 4 floats at a time using SIMD
  __m128 value_vec = _mm_set1_ps(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < frames_to_process - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    __m128 input_vec = _mm_loadu_ps(&values[processed_frames]);
    // Compare the 4 floats with the value
    __m128 cmp_vec = _mm_cmpneq_ps(input_vec, value_vec);
    // Check if any of the floats are not equal to the value
    if (_mm_movemask_ps(cmp_vec) != 0) {
      return false;
    }
  }
#elif defined(__ARM_NEON__)
  // Process 4 floats at a time using SIMD
  float32x4_t value_vec = vdupq_n_f32(value);
  // Start at 0 for byte alignment
  for (processed_frames = 0; processed_frames < frames_to_process - 3;
       processed_frames += 4) {
    // Load 4 floats from memory
    float32x4_t input_vec = vld1q_f32(&values[processed_frames]);
    // Compare the 4 floats with the value
    uint32x4_t cmp_vec = vceqq_f32(input_vec, value_vec);
    // Accumulate the elements of the cmp_vec vector using bitwise AND
    uint32x2_t cmp_reduced_32 =
        vand_u32(vget_low_u32(cmp_vec), vget_high_u32(cmp_vec));
    // Check if any of the floats are not equal to the value
    if (vget_lane_u32(vpmin_u32(cmp_reduced_32, cmp_reduced_32), 0) == 0) {
      return false;
    }
  }
#endif
  // Fallback implementation without SIMD optimization
  while (processed_frames < frames_to_process) {
    if (values[processed_frames] != value) {
      return false;
    }
    processed_frames++;
  }
  return true;
}

}  // namespace

class BiquadDSPKernel;

class BiquadProcessor final {
 public:
  BiquadProcessor(float sample_rate,
                  uint32_t number_of_channels,
                  unsigned render_quantum_frames,
                  AudioParamHandler& frequency,
                  AudioParamHandler& q,
                  AudioParamHandler& gain,
                  AudioParamHandler& detune);
  ~BiquadProcessor();

  std::unique_ptr<BiquadDSPKernel> CreateKernel();

  void Initialize();
  void Uninitialize();
  void Process(const AudioBus* source,
               AudioBus* destination,
               uint32_t frames_to_process);
  void ProcessOnlyAudioParams(uint32_t frames_to_process);
  void Reset();

  bool IsInitialized() const { return is_initialized_; }

  float SampleRate() const { return sample_rate_; }

  unsigned RenderQuantumFrames() const { return render_quantum_frames_; }

  double TailTime() const;
  double LatencyTime() const;
  bool RequiresTailProcessing() const;

  void SetNumberOfChannels(unsigned);
  unsigned NumberOfChannels() const { return number_of_channels_; }

  // Get the magnitude and phase response of the filter at the given
  // set of frequencies (in Hz). The phase response is in radians.
  void GetFrequencyResponse(int n_frequencies,
                            const float* frequency_hz,
                            float* mag_response,
                            float* phase_response);

  void CheckForDirtyCoefficients();

  bool AreFilterCoefficientsDirty() const {
    return are_filter_coefficients_dirty_;
  }
  bool HasSampleAccurateValues() const { return has_sample_accurate_values_; }
  bool IsAudioRate() const { return is_audio_rate_; }

  AudioParamHandler& Parameter1() { return *parameter1_; }
  AudioParamHandler& Parameter2() { return *parameter2_; }
  AudioParamHandler& Parameter3() { return *parameter3_; }
  AudioParamHandler& Parameter4() { return *parameter4_; }

  V8BiquadFilterType::Enum Type() const { return type_; }
  void SetType(V8BiquadFilterType::Enum type);

 private:
  V8BiquadFilterType::Enum type_ = V8BiquadFilterType::Enum::kLowpass;

  scoped_refptr<AudioParamHandler> parameter1_;
  scoped_refptr<AudioParamHandler> parameter2_;
  scoped_refptr<AudioParamHandler> parameter3_;
  scoped_refptr<AudioParamHandler> parameter4_;

  // so DSP kernels know when to re-compute coefficients
  bool are_filter_coefficients_dirty_ = true;

  // Set to true if any of the filter parameters are sample-accurate.
  bool has_sample_accurate_values_ = false;

  // Set to true if any of the filter parameters are a-rate.
  bool is_audio_rate_;

  bool has_just_reset_ = true;

  // Cache previous parameter values to allow us to skip recomputing filter
  // coefficients when parameters are not changing
  float previous_parameter1_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter2_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter3_ = std::numeric_limits<float>::quiet_NaN();
  float previous_parameter4_ = std::numeric_limits<float>::quiet_NaN();

  bool is_initialized_ = false;
  unsigned number_of_channels_;
  float sample_rate_;
  unsigned render_quantum_frames_;

  Vector<std::unique_ptr<BiquadDSPKernel>> kernels_ GUARDED_BY(process_lock_);
  mutable base::Lock process_lock_;
};

// BiquadDSPKernel is is responsible for filtering one channel of a
// BiquadProcessor using a Biquad object.
class BiquadDSPKernel final {
 public:
  explicit BiquadDSPKernel(BiquadProcessor* processor)
      : biquad_(processor->RenderQuantumFrames()),
        tail_time_(std::numeric_limits<double>::infinity()),
        kernel_processor_(processor),
        sample_rate_(processor->SampleRate()),
        render_quantum_frames_(processor->RenderQuantumFrames()) {}

  // AudioDSPKernel
  void Process(const float* source, float* dest, uint32_t frames_to_process);
  void ProcessOnlyAudioParams(uint32_t frames_to_process) {}
  void Reset() { biquad_.Reset(); }

  float SampleRate() const { return sample_rate_; }
  unsigned RenderQuantumFrames() const { return render_quantum_frames_; }
  double Nyquist() const { return 0.5 * SampleRate(); }

  BiquadProcessor* Processor() { return kernel_processor_; }
  const BiquadProcessor* Processor() const { return kernel_processor_; }

  // Get the magnitude and phase response of the given BiquadDSPKernel at the
  // given set of frequencies (in Hz). The phase response is in radians.  This
  // must be called from the main thread.
  static void GetFrequencyResponse(BiquadDSPKernel& kernel,
                                   int n_frequencies,
                                   const float* frequency_hz,
                                   float* mag_response,
                                   float* phase_response);

  bool RequiresTailProcessing() const;
  double TailTime() const;
  double LatencyTime() const;
  // Update the biquad coefficients with the given parameters
  void UpdateCoefficients(int number_of_frames,
                          const float* frequency,
                          const float* q,
                          const float* gain,
                          const float* detune);

 protected:
  BiquadProcessor* GetBiquadProcessor() {
    return static_cast<BiquadProcessor*>(Processor());
  }

  void UpdateCoefficientsIfNecessary(int)
      EXCLUSIVE_LOCKS_REQUIRED(process_lock_);

  Biquad biquad_;

 private:
  // Compute the tail time using the BiquadFilter coefficients at
  // index `coef_index`.
  void UpdateTailTime(int coef_index);

  // Synchronize process() with getting and setting the filter coefficients.
  mutable base::Lock process_lock_;

  // The current tail time for biquad filter.
  double tail_time_;

  // This raw pointer is safe because the AudioDSPKernelProcessor object is
  // guaranteed to be kept alive while the AudioDSPKernel object is alive.
  raw_ptr<BiquadProcessor> kernel_processor_;
  float sample_rate_;
  unsigned render_quantum_frames_;
};

void BiquadDSPKernel::UpdateCoefficientsIfNecessary(int frames_to_process) {
  if (GetBiquadProcessor()->AreFilterCoefficientsDirty()) {
    // TODO(crbug.com/40637820): Eventually, the render quantum size will no
    // longer be hardcoded as 128. At that point, we'll need to switch from
    // stack allocation to heap allocation.
    CHECK_EQ(RenderQuantumFrames(), kRenderQuantumFramesExpected);
    float cutoff_frequency[kRenderQuantumFramesExpected];
    float q[kRenderQuantumFramesExpected];
    float gain[kRenderQuantumFramesExpected];
    float detune[kRenderQuantumFramesExpected];  // in Cents

    SECURITY_CHECK(static_cast<unsigned>(frames_to_process) <=
                   RenderQuantumFrames());

    if (GetBiquadProcessor()->HasSampleAccurateValues() &&
        GetBiquadProcessor()->IsAudioRate()) {
      GetBiquadProcessor()->Parameter1().CalculateSampleAccurateValues(
          base::span(cutoff_frequency)
              .first(static_cast<size_t>(frames_to_process)));
      GetBiquadProcessor()->Parameter2().CalculateSampleAccurateValues(
          base::span(q).first(static_cast<size_t>(frames_to_process)));
      GetBiquadProcessor()->Parameter3().CalculateSampleAccurateValues(
          base::span(gain).first(static_cast<size_t>(frames_to_process)));
      GetBiquadProcessor()->Parameter4().CalculateSampleAccurateValues(
          base::span(detune).first(static_cast<size_t>(frames_to_process)));

      // If all the values are actually constant for this render (or the
      // automation rate is "k-rate" for all of the AudioParams), we don't need
      // to compute filter coefficients for each frame since they would be the
      // same as the first.
      bool is_constant =
          HasConstantValues(cutoff_frequency, frames_to_process) &&
          HasConstantValues(q, frames_to_process) &&
          HasConstantValues(gain, frames_to_process) &&
          HasConstantValues(detune, frames_to_process);

      UpdateCoefficients(is_constant ? 1 : frames_to_process, cutoff_frequency,
                         q, gain, detune);
    } else {
      cutoff_frequency[0] = GetBiquadProcessor()->Parameter1().FinalValue();
      q[0] = GetBiquadProcessor()->Parameter2().FinalValue();
      gain[0] = GetBiquadProcessor()->Parameter3().FinalValue();
      detune[0] = GetBiquadProcessor()->Parameter4().FinalValue();
      UpdateCoefficients(1, cutoff_frequency, q, gain, detune);
    }
  }
}

void BiquadDSPKernel::UpdateCoefficients(int number_of_frames,
                                         const float* cutoff_frequency,
                                         const float* q,
                                         const float* gain,
                                         const float* detune) {
  // Convert from Hertz to normalized frequency 0 -> 1.
  double nyquist = Nyquist();

  biquad_.SetHasSampleAccurateValues(number_of_frames > 1);

  for (int k = 0; k < number_of_frames; ++k) {
    double normalized_frequency = cutoff_frequency[k] / nyquist;

    // Offset frequency by detune.
    if (detune[k]) {
      // Detune multiplies the frequency by 2^(detune[k] / 1200).
      normalized_frequency *= exp2(detune[k] / 1200);
    }

    // Configure the biquad with the new filter parameters for the appropriate
    // type of filter.
    switch (GetBiquadProcessor()->Type()) {
      case V8BiquadFilterType::Enum::kLowpass:
        biquad_.SetLowpassParams(k, normalized_frequency, q[k]);
        break;

      case V8BiquadFilterType::Enum::kHighpass:
        biquad_.SetHighpassParams(k, normalized_frequency, q[k]);
        break;

      case V8BiquadFilterType::Enum::kBandpass:
        biquad_.SetBandpassParams(k, normalized_frequency, q[k]);
        break;

      case V8BiquadFilterType::Enum::kLowshelf:
        biquad_.SetLowShelfParams(k, normalized_frequency, gain[k]);
        break;

      case V8BiquadFilterType::Enum::kHighshelf:
        biquad_.SetHighShelfParams(k, normalized_frequency, gain[k]);
        break;

      case V8BiquadFilterType::Enum::kPeaking:
        biquad_.SetPeakingParams(k, normalized_frequency, q[k], gain[k]);
        break;

      case V8BiquadFilterType::Enum::kNotch:
        biquad_.SetNotchParams(k, normalized_frequency, q[k]);
        break;

      case V8BiquadFilterType::Enum::kAllpass:
        biquad_.SetAllpassParams(k, normalized_frequency, q[k]);
        break;
    }
  }

  UpdateTailTime(number_of_frames - 1);
}

void BiquadDSPKernel::UpdateTailTime(int coef_index) {
  // TODO(crbug.com/40268882): A reasonable upper limit for the tail time. While
  // it's easy to create biquad filters whose tail time can be much larger than
  // this, limit the maximum to this value so that we don't keep such nodes
  // alive "forever". Investigate if we can adjust this to a smaller value.
  constexpr double kMaxTailTime = 30.0;

  double sample_rate = SampleRate();
  double tail =
      biquad_.TailFrame(coef_index, kMaxTailTime * sample_rate) / sample_rate;

  tail_time_ = ClampTo(tail, 0.0, kMaxTailTime);
}

void BiquadDSPKernel::Process(const float* source,
                              float* destination,
                              uint32_t frames_to_process) {
  DCHECK(source);
  DCHECK(destination);
  DCHECK(GetBiquadProcessor());

  // Recompute filter coefficients if any of the parameters have changed.
  // FIXME: as an optimization, implement a way that a Biquad object can simply
  // copy its internal filter coefficients from another Biquad object.  Then
  // re-factor this code to only run for the first BiquadDSPKernel of each
  // BiquadProcessor.

  // The audio thread can't block on this lock; skip updating the coefficients
  // for this block if necessary. We'll get them the next time around.
  {
    base::AutoTryLock try_locker(process_lock_);
    if (try_locker.is_acquired()) {
      UpdateCoefficientsIfNecessary(frames_to_process);
    }
  }

  biquad_.Process(source, destination, frames_to_process);
}

void BiquadDSPKernel::GetFrequencyResponse(BiquadDSPKernel& kernel,
                                           int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  // Only allow on the main thread because we don't want the audio thread to be
  // updating `kernel` while we're computing the response.
  DCHECK(IsMainThread());

  DCHECK_GE(n_frequencies, 0);
  DCHECK(frequency_hz);
  DCHECK(mag_response);
  DCHECK(phase_response);

  Vector<float> frequency(n_frequencies);
  double nyquist = kernel.Nyquist();

  // Convert from frequency in Hz to normalized frequency (0 -> 1),
  // with 1 equal to the Nyquist frequency.
  for (int k = 0; k < n_frequencies; ++k) {
    frequency[k] = frequency_hz[k] / nyquist;
  }

  kernel.biquad_.GetFrequencyResponse(n_frequencies, frequency.data(),
                                      mag_response, phase_response);
}

bool BiquadDSPKernel::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both
  // be zero. This is for simplicity and because TailTime() is 0
  // basically only when the filter response H(z) = 0 or H(z) = 1. And
  // it's ok to return true. It just means the node lives a little
  // longer than strictly necessary.
  return true;
}

double BiquadDSPKernel::TailTime() const {
  return tail_time_;
}

double BiquadDSPKernel::LatencyTime() const {
  return 0;
}

BiquadProcessor::BiquadProcessor(float sample_rate,
                                 uint32_t number_of_channels,
                                 unsigned render_quantum_frames,
                                 AudioParamHandler& frequency,
                                 AudioParamHandler& q,
                                 AudioParamHandler& gain,
                                 AudioParamHandler& detune)
    : parameter1_(&frequency),
      parameter2_(&q),
      parameter3_(&gain),
      parameter4_(&detune),
      number_of_channels_(number_of_channels),
      sample_rate_(sample_rate),
      render_quantum_frames_(render_quantum_frames) {}

BiquadProcessor::~BiquadProcessor() {
  if (IsInitialized()) {
    Uninitialize();
  }
}

std::unique_ptr<BiquadDSPKernel> BiquadProcessor::CreateKernel() {
  return std::make_unique<BiquadDSPKernel>(this);
}

void BiquadProcessor::CheckForDirtyCoefficients() {
  // The BiquadDSPKernel objects rely on this value to see if they need to
  // re-compute their internal filter coefficients. Start out assuming filter
  // parameters are not changing.
  are_filter_coefficients_dirty_ = false;
  has_sample_accurate_values_ = false;

  if (parameter1_->HasSampleAccurateValues() ||
      parameter2_->HasSampleAccurateValues() ||
      parameter3_->HasSampleAccurateValues() ||
      parameter4_->HasSampleAccurateValues()) {
    // Coefficients are dirty if any of them has automations or if there are
    // connections to the AudioParam.
    are_filter_coefficients_dirty_ = true;
    has_sample_accurate_values_ = true;
    // If any parameter is a-rate, then the filter must do a-rate processing for
    // everything.
    is_audio_rate_ = parameter1_->IsAudioRate() || parameter2_->IsAudioRate() ||
                     parameter3_->IsAudioRate() || parameter4_->IsAudioRate();
  } else {
    if (has_just_reset_) {
      // Snap to exact values first time after reset
      previous_parameter1_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter2_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter3_ = std::numeric_limits<float>::quiet_NaN();
      previous_parameter4_ = std::numeric_limits<float>::quiet_NaN();
      are_filter_coefficients_dirty_ = true;
      has_just_reset_ = false;
    } else {
      // If filter parameters have changed then mark coefficients as dirty.
      const float parameter1_final = parameter1_->FinalValue();
      const float parameter2_final = parameter2_->FinalValue();
      const float parameter3_final = parameter3_->FinalValue();
      const float parameter4_final = parameter4_->FinalValue();
      if ((previous_parameter1_ != parameter1_final) ||
          (previous_parameter2_ != parameter2_final) ||
          (previous_parameter3_ != parameter3_final) ||
          (previous_parameter4_ != parameter4_final)) {
        are_filter_coefficients_dirty_ = true;
        previous_parameter1_ = parameter1_final;
        previous_parameter2_ = parameter2_final;
        previous_parameter3_ = parameter3_final;
        previous_parameter4_ = parameter4_final;
      }
    }
  }
}

void BiquadProcessor::Initialize() {
  if (IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  DCHECK(!kernels_.size());

  // Create processing kernels, one per channel.
  for (unsigned i = 0; i < NumberOfChannels(); ++i) {
    kernels_.push_back(CreateKernel());
  }

  is_initialized_ = true;

  has_just_reset_ = true;
}

void BiquadProcessor::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  kernels_.clear();

  is_initialized_ = false;
}

void BiquadProcessor::Process(const AudioBus* source,
                              AudioBus* destination,
                              uint32_t frames_to_process) {
  if (!IsInitialized()) {
    destination->Zero();
    return;
  }

  // Synchronize with possible dynamic changes to the impulse response.
  base::AutoTryLock try_locker(process_lock_);
  if (!try_locker.is_acquired()) {
    // Can't get the lock. We must be in the middle of changing something.
    destination->Zero();
    return;
  }

  CheckForDirtyCoefficients();

  // For each channel of our input, process using the corresponding
  // BiquadDSPKernel into the output channel.
  for (unsigned i = 0; i < kernels_.size(); ++i) {
    kernels_[i]->Process(source->Channel(i)->Data(),
                         destination->Channel(i)->MutableData(),
                         frames_to_process);
  }
}

void BiquadProcessor::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  // TODO(crbug.com/40637820): Eventually, the render quantum size will no
  // longer be hardcoded as 128. At that point, we'll need to switch from
  // stack allocation to heap allocation.
  CHECK_EQ(RenderQuantumFrames(), kRenderQuantumFramesExpected);

  DCHECK_LE(frames_to_process, kRenderQuantumFramesExpected);

  float values[kRenderQuantumFramesExpected];

  parameter1_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter2_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter3_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
  parameter4_->CalculateSampleAccurateValues(
      base::span(values).first(frames_to_process));
}

void BiquadProcessor::Reset() {
  DCHECK(IsMainThread());
  if (!IsInitialized()) {
    return;
  }

  base::AutoLock locker(process_lock_);
  for (auto& kernel : kernels_) {
    kernel->Reset();
  }

  has_just_reset_ = true;
}

void BiquadProcessor::SetNumberOfChannels(unsigned number_of_channels) {
  if (number_of_channels == number_of_channels_) {
    return;
  }

  DCHECK(!IsInitialized());
  number_of_channels_ = number_of_channels;
}

bool BiquadProcessor::RequiresTailProcessing() const {
  // Always return true even if the tail time and latency might both be zero.
  return true;
}

double BiquadProcessor::TailTime() const {
  DCHECK(!IsMainThread());
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    // It is expected that all the kernels have the same tailTime.
    return !kernels_.empty() ? kernels_.front()->TailTime() : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

double BiquadProcessor::LatencyTime() const {
  DCHECK(!IsMainThread());
  base::AutoTryLock try_locker(process_lock_);
  if (try_locker.is_acquired()) {
    // It is expected that all the kernels have the same latencyTime.
    return !kernels_.empty() ? kernels_.front()->LatencyTime() : 0;
  }
  // Since we don't want to block the Audio Device thread, we return a large
  // value instead of trying to acquire the lock.
  return std::numeric_limits<double>::infinity();
}

void BiquadProcessor::SetType(V8BiquadFilterType::Enum type) {
  if (type != type_) {
    type_ = type;
    Reset();  // The filter state must be reset only if the type has changed.
  }
}

void BiquadProcessor::GetFrequencyResponse(int n_frequencies,
                                           const float* frequency_hz,
                                           float* mag_response,
                                           float* phase_response) {
  DCHECK(IsMainThread());

  // Compute the frequency response on a separate temporary kernel
  // to avoid interfering with the processing running in the audio
  // thread on the main kernels.

  std::unique_ptr<BiquadDSPKernel> response_kernel =
      std::make_unique<BiquadDSPKernel>(this);

  float cutoff_frequency;
  float q;
  float gain;
  float detune;  // in Cents

  {
    // Get a copy of the current biquad filter coefficients so we can update
    // `response_kernel` with these values.  We need to synchronize with
    // `Process()` to prevent process() from updating the filter coefficients
    // while we're trying to access them.  Since this is on the main thread, we
    // can wait.  The audio thread will update the coefficients the next time
    // around, it it were blocked.
    base::AutoLock process_locker(process_lock_);

    cutoff_frequency = Parameter1().Value();
    q = Parameter2().Value();
    gain = Parameter3().Value();
    detune = Parameter4().Value();
  }

  response_kernel->UpdateCoefficients(1, &cutoff_frequency, &q, &gain, &detune);
  BiquadDSPKernel::GetFrequencyResponse(*response_kernel, n_frequencies,
                                        frequency_hz, mag_response,
                                        phase_response);
}

BiquadFilterHandler::BiquadFilterHandler(AudioNode& node,
                                         float sample_rate,
                                         AudioParamHandler& frequency,
                                         AudioParamHandler& q,
                                         AudioParamHandler& gain,
                                         AudioParamHandler& detune)
    : AudioHandler(NodeType::kNodeTypeBiquadFilter, node, sample_rate),
      processor_(std::make_unique<BiquadProcessor>(
          sample_rate,
          kNumberOfChannels,
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
          frequency,
          q,
          gain,
          detune)) {
  DCHECK(Context());
  DCHECK(Context()->GetExecutionContext());

  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
      TaskType::kMediaElementEvent);

  // Initialize the handler so that AudioParams can be processed.
  Initialize();
}

scoped_refptr<BiquadFilterHandler> BiquadFilterHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& frequency,
    AudioParamHandler& q,
    AudioParamHandler& gain,
    AudioParamHandler& detune) {
  return base::AdoptRef(
      new BiquadFilterHandler(node, sample_rate, frequency, q, gain, detune));
}

void BiquadFilterHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  DCHECK(processor_);
  processor_->Initialize();

  AudioHandler::Initialize();
}

void BiquadFilterHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  DCHECK(processor_);
  processor_->Uninitialize();

  AudioHandler::Uninitialize();
}

void BiquadFilterHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "BiquadFilterHandler::Process");

  AudioBus* destination_bus = Output(0).Bus();

  if (!IsInitialized() || !processor_ ||
      processor_->NumberOfChannels() != NumberOfChannels()) {
    destination_bus->Zero();
  } else {
    scoped_refptr<AudioBus> source_bus = Input(0).Bus();

    // FIXME: if we take "tail time" into account, then we can avoid calling
    // processor()->process() once the tail dies down.
    if (!Input(0).IsConnected()) {
      source_bus->Zero();
    }

    processor_->Process(source_bus.get(), destination_bus, frames_to_process);
  }

  if (!did_warn_bad_filter_state_) {
    // Inform the user once if the output has a non-finite value.  This is a
    // proxy for the filter state containing non-finite values since the output
    // is also saved as part of the state of the filter.
    if (HasNonFiniteOutput()) {
      did_warn_bad_filter_state_ = true;

      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&BiquadFilterHandler::NotifyBadState,
                              weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void BiquadFilterHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized() || !processor_) {
    return;
  }

  processor_->ProcessOnlyAudioParams(frames_to_process);
}

// Nice optimization in the very common case allowing for "in-place" processing
void BiquadFilterHandler::PullInputs(uint32_t frames_to_process) {
  // Render input stream - suggest to the input to render directly into output
  // bus for in-place processing in process() if possible.
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

// As soon as we know the channel count of our input, we can lazily initialize.
// Sometimes this may be called more than once with different channel counts, in
// which case we must safely uninitialize and then re-initialize with the new
// channel count.
void BiquadFilterHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();

  DCHECK_EQ(input, &Input(0));
  DCHECK(processor_);

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
    processor_->SetNumberOfChannels(number_of_channels);
    Initialize();
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
}

unsigned BiquadFilterHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

void BiquadFilterHandler::GetFrequencyResponse(int n_frequencies,
                                               const float* frequency_hz,
                                               float* mag_response,
                                               float* phase_response) {
  processor_->GetFrequencyResponse(n_frequencies, frequency_hz, mag_response,
                                   phase_response);
}

V8BiquadFilterType::Enum BiquadFilterHandler::Type() const {
  return processor_->Type();
}

void BiquadFilterHandler::SetType(V8BiquadFilterType::Enum type) {
  processor_->SetType(type);
}

bool BiquadFilterHandler::RequiresTailProcessing() const {
  return processor_->RequiresTailProcessing();
}

double BiquadFilterHandler::TailTime() const {
  return processor_->TailTime();
}

double BiquadFilterHandler::LatencyTime() const {
  return processor_->LatencyTime();
}

bool BiquadFilterHandler::HasNonFiniteOutput() const {
  AudioBus* output_bus = Output(0).Bus();

  for (wtf_size_t k = 0; k < output_bus->NumberOfChannels(); ++k) {
    AudioChannel* channel = output_bus->Channel(k);
    if (channel->length() > 0 && !std::isfinite(channel->Data()[0])) {
      return true;
    }
  }

  return false;
}

void BiquadFilterHandler::NotifyBadState() const {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  Context()->GetExecutionContext()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          StrCat({NodeTypeName(),
                  ": state is bad, probably due to unstable filter caused by "
                  "fast parameter automation."})));
}

bool BiquadFilterHandler::HasConstantValuesForTesting(float* values,
                                                      int frames_to_process) {
  return HasConstantValues(values, frames_to_process);
}

}  // namespace blink

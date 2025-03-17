// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "third_party/blink/renderer/modules/webaudio/wave_shaper_handler.h"

#include <algorithm>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_array.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel.h"
#include "third_party/blink/renderer/platform/audio/audio_dsp_kernel_processor.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/down_sampler.h"
#include "third_party/blink/renderer/platform/audio/up_sampler.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <xmmintrin.h>
#elif defined(CPU_ARM_NEON)
#include <arm_neon.h>
#endif

namespace blink {

namespace {

constexpr unsigned kDefaultNumberOfOutputChannels = 1;

// Computes value of the WaveShaper
double WaveShaperCurveValue(float input,
                            const float* curve_data,
                            int curve_length) {
  // Calculate a virtual index based on input -1 -> +1 with -1 being
  // curve[0], +1 being curve[curveLength - 1], and 0 being at the center of
  // the curve data. Then linearly interpolate between the two points in the
  // curve.
  const double virtual_index = 0.5 * (input + 1) * (curve_length - 1);
  double output;
  if (virtual_index < 0) {
    // input < -1, so use curve[0]
    output = curve_data[0];
  } else if (virtual_index >= curve_length - 1) {
    // input >= 1, so use last curve value
    output = curve_data[curve_length - 1];
  } else {
    // The general case where -1 <= input < 1, where 0 <= virtualIndex <
    // curveLength - 1, so interpolate between the nearest samples on the
    // curve.
    const unsigned index1 = static_cast<unsigned>(virtual_index);
    const unsigned index2 = index1 + 1;
    const double interpolation_factor = virtual_index - index1;

    const double value1 = curve_data[index1];
    const double value2 = curve_data[index2];

    output =
        (1.0 - interpolation_factor) * value1 + interpolation_factor * value2;
  }

  return output;
}

}  // namespace

// WaveShaperProcessor is an AudioDSPKernelProcessor which uses
// WaveShaperDSPKernel objects to implement non-linear distortion effects.
class WaveShaperProcessor final : public AudioDSPKernelProcessor {
 public:
  WaveShaperProcessor(float sample_rate,
                      unsigned number_of_channels,
                      unsigned render_quantum_frames)
      : AudioDSPKernelProcessor(sample_rate,
                                number_of_channels,
                                render_quantum_frames) {}

  ~WaveShaperProcessor() override {
    if (IsInitialized()) {
      Uninitialize();
    }
  }

  std::unique_ptr<AudioDSPKernel> CreateKernel() override {
    return std::make_unique<WaveShaperDSPKernel>(this);
  }

  void Process(const AudioBus* source,
               AudioBus* destination,
               uint32_t frames_to_process) override {
    if (!IsInitialized()) {
      destination->Zero();
      return;
    }

    DCHECK_EQ(source->NumberOfChannels(), destination->NumberOfChannels());

    // The audio thread can't block on this lock, so we call tryLock() instead.
    base::AutoTryLock try_locker(process_lock_);
    if (try_locker.is_acquired()) {
      DCHECK_EQ(source->NumberOfChannels(), kernels_.size());
      // For each channel of our input, process using the corresponding
      // WaveShaperDSPKernel into the output channel.
      for (unsigned i = 0; i < kernels_.size(); ++i) {
        kernels_[i]->Process(source->Channel(i)->Data(),
                             destination->Channel(i)->MutableData(),
                             frames_to_process);
      }
    } else {
      // Too bad - the tryLock() failed. We must be in the middle of a
      // setCurve() call.
      destination->Zero();
    }
  }

  void SetCurve(const float* curve_data, unsigned curve_length) {
    DCHECK(IsMainThread());

    // This synchronizes with process().
    base::AutoLock process_locker(process_lock_);

    if (curve_length == 0 || !curve_data) {
      curve_ = nullptr;
      return;
    }

    // Copy the curve data, if any, to our internal buffer.
    curve_ = std::make_unique<Vector<float>>(curve_length);
    memcpy(curve_->data(), curve_data, sizeof(float) * curve_length);

    DCHECK_GE(kernels_.size(), 1ULL);

    // Compute the curve output for a zero input, and set the tail time for all
    // the kernels.
    WaveShaperDSPKernel* kernel =
        static_cast<WaveShaperDSPKernel*>(kernels_[0].get());
    double output = WaveShaperCurveValue(0.0, curve_data, curve_length);
    double tail_time =
        output == 0 ? 0 : std::numeric_limits<double>::infinity();

    for (auto& k : kernels_) {
      kernel = static_cast<WaveShaperDSPKernel*>(k.get());
      kernel->SetTailTime(tail_time);
    }
  }

  const Vector<float>* Curve() const { return curve_.get(); }

  void SetOversample(V8OverSampleType::Enum oversample) {
    // This synchronizes with process().
    base::AutoLock process_locker(process_lock_);

    oversample_ = oversample;

    if (oversample != V8OverSampleType::Enum::kNone) {
      for (auto& i : kernels_) {
        WaveShaperDSPKernel* kernel =
            static_cast<WaveShaperDSPKernel*>(i.get());
        kernel->LazyInitializeOversampling();
      }
    }
  }

  V8OverSampleType::Enum Oversample() const { return oversample_; }

 private:
  // WaveShaperDSPKernel is an AudioDSPKernel and is responsible for non-linear
  // distortion on one channel.
  class WaveShaperDSPKernel final : public AudioDSPKernel {
   public:
    explicit WaveShaperDSPKernel(WaveShaperProcessor* processor)
        : AudioDSPKernel(processor),
          // 4 times render size to handle 4x oversampling.
          virtual_index_(4 * RenderQuantumFrames()),
          index_(4 * RenderQuantumFrames()),
          v1_(4 * RenderQuantumFrames()),
          v2_(4 * RenderQuantumFrames()),
          f_(4 * RenderQuantumFrames()) {
      if (processor->Oversample() != V8OverSampleType::Enum::kNone) {
        LazyInitializeOversampling();
      }
    }

    // AudioDSPKernel
    void Process(const float* source,
                 float* destination,
                 uint32_t frames_to_process) override {
      switch (GetWaveShaperProcessor()->Oversample()) {
        case V8OverSampleType::Enum::kNone:
          ProcessCurve(source, destination, frames_to_process);
          break;
        case V8OverSampleType::Enum::k2X:
          ProcessCurve2x(source, destination, frames_to_process);
          break;
        case V8OverSampleType::Enum::k4X:
          ProcessCurve4x(source, destination, frames_to_process);
          break;

        default:
          NOTREACHED();
      }
    }
    void Reset() override {
      if (up_sampler_) {
        up_sampler_->Reset();
        down_sampler_->Reset();
        up_sampler2_->Reset();
        down_sampler2_->Reset();
      }
    }
    double TailTime() const override { return tail_time_; }
    double LatencyTime() const override {
      size_t latency_frames = 0;
      WaveShaperDSPKernel* kernel = const_cast<WaveShaperDSPKernel*>(this);

      switch (kernel->GetWaveShaperProcessor()->Oversample()) {
        case V8OverSampleType::Enum::kNone:
          break;
        case V8OverSampleType::Enum::k2X:
          latency_frames += up_sampler_->LatencyFrames();
          latency_frames += down_sampler_->LatencyFrames();
          break;
        case V8OverSampleType::Enum::k4X: {
          // Account for first stage upsampling.
          latency_frames += up_sampler_->LatencyFrames();
          latency_frames += down_sampler_->LatencyFrames();

          // Account for second stage upsampling.
          // and divide by 2 to get back down to the regular sample-rate.
          size_t latency_frames2 = (up_sampler2_->LatencyFrames() +
                                    down_sampler2_->LatencyFrames()) /
                                   2;
          latency_frames += latency_frames2;
          break;
        }
        default:
          NOTREACHED();
      }

      return static_cast<double>(latency_frames) / SampleRate();
    }

    bool RequiresTailProcessing() const override {
      // Always return true even if the tail time and latency might both be
      // zero.
      return true;
    }

    // Oversampling requires more resources, so let's only allocate them if
    // needed.
    void LazyInitializeOversampling() {
      if (!temp_buffer_) {
        temp_buffer_ =
            std::make_unique<AudioFloatArray>(RenderQuantumFrames() * 2);
        temp_buffer2_ =
            std::make_unique<AudioFloatArray>(RenderQuantumFrames() * 4);
        up_sampler_ = std::make_unique<UpSampler>(RenderQuantumFrames());
        down_sampler_ =
            std::make_unique<DownSampler>(RenderQuantumFrames() * 2);
        up_sampler2_ = std::make_unique<UpSampler>(RenderQuantumFrames() * 2);
        down_sampler2_ =
            std::make_unique<DownSampler>(RenderQuantumFrames() * 4);
      }
    }

    // Like WaveShaperCurveValue, but computes the values for a vector of
    // inputs.
    void WaveShaperCurveValues(float* destination,
                               const float* source,
                               uint32_t frames_to_process,
                               const float* curve_data,
                               int curve_length) const {
      DCHECK_LE(frames_to_process, virtual_index_.size());
      // Index into the array computed from the source value.
      float* virtual_index = virtual_index_.Data();

      // virtual_index[k] =
      //   ClampTo(0.5 * (source[k] + 1) * (curve_length - 1),
      //           0.0f,
      //           static_cast<float>(curve_length - 1))

      // Add 1 to source puttting  result in virtual_index
      vector_math::Vsadd(source, 1, 1, virtual_index, 1, frames_to_process);

      // Scale virtual_index in place by (curve_lenth -1)/2
      vector_math::Vsmul(virtual_index, 1, 0.5 * (curve_length - 1),
                         virtual_index, 1, frames_to_process);

      // Clip virtual_index, in place.
      vector_math::Vclip(virtual_index, 1, 0, curve_length - 1, virtual_index,
                         1, frames_to_process);

      // index = floor(virtual_index)
      DCHECK_LE(frames_to_process, index_.size());
      float* index = index_.Data();

      // v1 and v2 hold the curve_data corresponding to the closest curve
      // values to the source sample.  To save memory, v1 will use the
      // destination array.
      DCHECK_LE(frames_to_process, v1_.size());
      DCHECK_LE(frames_to_process, v2_.size());
      float* v1 = v1_.Data();
      float* v2 = v2_.Data();

      // Interpolation factor: virtual_index - index.
      DCHECK_LE(frames_to_process, f_.size());
      float* f = f_.Data();

      int max_index = curve_length - 1;
      unsigned k = 0;
#if defined(ARCH_CPU_X86_FAMILY)
      {
        int loop_limit = frames_to_process / 4;

        // one = 1
        __m128i one = _mm_set1_epi32(1);

        // Do 4 eleemnts at a time
        for (int loop = 0; loop < loop_limit; ++loop, k += 4) {
          // v = virtual_index[k]
          __m128 v = _mm_loadu_ps(virtual_index + k);

          // index1 = static_cast<int>(v);
          __m128i index1 = _mm_cvttps_epi32(v);

          // v = static_cast<float>(index1) and save result to index[k:k+3]
          v = _mm_cvtepi32_ps(index1);
          _mm_storeu_ps(&index[k], v);

          // index2 = index2 + 1;
          __m128i index2 = _mm_add_epi32(index1, one);

          // Convert index1/index2 to arrays of 32-bit int values that are our
          // array indices to use to get the curve data.
          int32_t* i1 = reinterpret_cast<int32_t*>(&index1);
          int32_t* i2 = reinterpret_cast<int32_t*>(&index2);

          // Get the curve_data values and save them in v1 and v2,
          // carefully clamping the values.  If the input is NaN, index1
          // could be 0x8000000.
          v1[k] = curve_data[ClampTo(i1[0], 0, max_index)];
          v2[k] = curve_data[ClampTo(i2[0], 0, max_index)];
          v1[k + 1] = curve_data[ClampTo(i1[1], 0, max_index)];
          v2[k + 1] = curve_data[ClampTo(i2[1], 0, max_index)];
          v1[k + 2] = curve_data[ClampTo(i1[2], 0, max_index)];
          v2[k + 2] = curve_data[ClampTo(i2[2], 0, max_index)];
          v1[k + 3] = curve_data[ClampTo(i1[3], 0, max_index)];
          v2[k + 3] = curve_data[ClampTo(i2[3], 0, max_index)];
        }
      }
#elif defined(CPU_ARM_NEON)
      {
        int loop_limit = frames_to_process / 4;

        // Neon constants:
        //   zero = 0
        //   one  = 1
        //   max  = max_index
        int32x4_t zero = vdupq_n_s32(0);
        int32x4_t one = vdupq_n_s32(1);
        int32x4_t max = vdupq_n_s32(max_index);

        for (int loop = 0; loop < loop_limit; ++loop, k += 4) {
          // v = virtual_index
          float32x4_t v = vld1q_f32(virtual_index + k);

          // index1 = static_cast<int32_t>(v), then clamp to a valid index range
          // for curve_data
          int32x4_t index1 = vcvtq_s32_f32(v);
          index1 = vmaxq_s32(vminq_s32(index1, max), zero);

          // v = static_cast<float>(v) and save it away for later use.
          v = vcvtq_f32_s32(index1);
          vst1q_f32(&index[k], v);

          // index2 = index1 + 1, then clamp to a valid range for curve_data.
          int32x4_t index2 = vaddq_s32(index1, one);
          index2 = vmaxq_s32(vminq_s32(index2, max), zero);

          // Save index1/2 so we can get the individual parts.  Aligned to
          // 16 bytes for vst1q instruction.
          int32_t i1[4] __attribute__((aligned(16)));
          int32_t i2[4] __attribute__((aligned(16)));
          vst1q_s32(i1, index1);
          vst1q_s32(i2, index2);

          // Get curve elements corresponding to the indices.
          v1[k] = curve_data[i1[0]];
          v2[k] = curve_data[i2[0]];
          v1[k + 1] = curve_data[i1[1]];
          v2[k + 1] = curve_data[i2[1]];
          v1[k + 2] = curve_data[i1[2]];
          v2[k + 2] = curve_data[i2[2]];
          v1[k + 3] = curve_data[i1[3]];
          v2[k + 3] = curve_data[i2[3]];
        }
      }
#endif

      // Compute values for index1 and load the curve_data corresponding to
      // indices.
      for (; k < frames_to_process; ++k) {
        unsigned index1 =
            ClampTo(static_cast<unsigned>(virtual_index[k]), 0, max_index);
        unsigned index2 = ClampTo(index1 + 1, 0, max_index);
        index[k] = index1;
        v1[k] = curve_data[index1];
        v2[k] = curve_data[index2];
      }

      // f[k] = virtual_index[k] - index[k]
      vector_math::Vsub(virtual_index, 1, index, 1, f, 1, frames_to_process);

      // Do the linear interpolation of the curve data:
      // destination[k] = v1[k] + f[k]*(v2[k] - v1[k])
      //
      // 1. v2[k] = v2[k] - v1[k]
      // 2. v2[k] = f[k]*v2[k] = f[k]*(v2[k] - v1[k])
      // 3. destination[k] = destination[k] + v2[k]
      //                   = v1[k] + f[k]*(v2[k] - v1[k])
      vector_math::Vsub(v2, 1, v1, 1, v2, 1, frames_to_process);
      vector_math::Vmul(f, 1, v2, 1, v2, 1, frames_to_process);
      vector_math::Vadd(v2, 1, v1, 1, destination, 1, frames_to_process);
    }

    // Set the tail time
    void SetTailTime(double time) { tail_time_ = time; }

   private:
    // Apply the shaping curve.
    void ProcessCurve(const float* source,
                      float* destination,
                      uint32_t frames_to_process) {
      DCHECK(source);
      DCHECK(destination);
      DCHECK(GetWaveShaperProcessor());

      const Vector<float>* curve = GetWaveShaperProcessor()->Curve();
      if (!curve) {
        // Act as "straight wire" pass-through if no curve is set.
        memcpy(destination, source, sizeof(float) * frames_to_process);
        return;
      }

      const float* curve_data = curve->data();
      int curve_length = curve->size();

      DCHECK(curve_data);

      if (!curve_data || !curve_length) {
        memcpy(destination, source, sizeof(float) * frames_to_process);
        return;
      }

      // Apply waveshaping curve.
      WaveShaperCurveValues(destination, source, frames_to_process, curve_data,
                            curve_length);
    }

    // Use up-sampling, process at the higher sample-rate, then down-sample.
    void ProcessCurve2x(const float* source,
                        float* destination,
                        uint32_t frames_to_process) {
      DCHECK_EQ(frames_to_process, RenderQuantumFrames());

      float* temp_p = temp_buffer_->Data();

      up_sampler_->Process(source, temp_p, frames_to_process);

      // Process at 2x up-sampled rate.
      ProcessCurve(temp_p, temp_p, frames_to_process * 2);

      down_sampler_->Process(temp_p, destination, frames_to_process * 2);
    }

    void ProcessCurve4x(const float* source,
                        float* destination,
                        uint32_t frames_to_process) {
      DCHECK_EQ(frames_to_process, RenderQuantumFrames());

      float* temp_p = temp_buffer_->Data();
      float* temp_p2 = temp_buffer2_->Data();

      up_sampler_->Process(source, temp_p, frames_to_process);
      up_sampler2_->Process(temp_p, temp_p2, frames_to_process * 2);

      // Process at 4x up-sampled rate.
      ProcessCurve(temp_p2, temp_p2, frames_to_process * 4);

      down_sampler2_->Process(temp_p2, temp_p, frames_to_process * 4);
      down_sampler_->Process(temp_p, destination, frames_to_process * 2);
    }

    WaveShaperProcessor* GetWaveShaperProcessor() {
      return static_cast<WaveShaperProcessor*>(Processor());
    }

    // Oversampling.
    std::unique_ptr<AudioFloatArray> temp_buffer_;
    std::unique_ptr<AudioFloatArray> temp_buffer2_;
    std::unique_ptr<UpSampler> up_sampler_;
    std::unique_ptr<DownSampler> down_sampler_;
    std::unique_ptr<UpSampler> up_sampler2_;
    std::unique_ptr<DownSampler> down_sampler2_;

    // Tail time for the WaveShaper.  This basically can have two values: 0 and
    // infinity.  It only takes the value of infinity if the wave shaper curve
    // is such that a zero input produces a non-zero output.  In this case, the
    // node has an infinite tail so that silent input continues to produce
    // non-silent output.
    double tail_time_ = 0;

    // Work arrays needed by WaveShaperCurveValues().  Mutable so this
    // const function can modify these arrays.  There's no state or
    // anything kept here.  See WaveShaperCurveValues() for details on
    // what these hold.
    mutable AudioFloatArray virtual_index_;
    mutable AudioFloatArray index_;
    mutable AudioFloatArray v1_;
    mutable AudioFloatArray v2_;
    mutable AudioFloatArray f_;
  };

  // m_curve represents the non-linear shaping curve.
  std::unique_ptr<Vector<float>> curve_;

  V8OverSampleType::Enum oversample_ = V8OverSampleType::Enum::kNone;
};

scoped_refptr<WaveShaperHandler> WaveShaperHandler::Create(AudioNode& node,
                                                           float sample_rate) {
  return base::AdoptRef(new WaveShaperHandler(node, sample_rate));
}

void WaveShaperHandler::SetCurve(const float* curve_data,
                                 unsigned curve_length) {
  DCHECK(IsMainThread());
  GetWaveShaperProcessor()->SetCurve(curve_data, curve_length);
}

const Vector<float>* WaveShaperHandler::Curve() const {
  DCHECK(IsMainThread());
  return GetWaveShaperProcessor()->Curve();
}

void WaveShaperHandler::SetOversample(V8OverSampleType::Enum oversample) {
  DCHECK(IsMainThread());
  GetWaveShaperProcessor()->SetOversample(oversample);
}

V8OverSampleType::Enum WaveShaperHandler::Oversample() const {
  DCHECK(IsMainThread());
  return GetWaveShaperProcessor()->Oversample();
}

WaveShaperHandler::WaveShaperHandler(AudioNode& node, float sample_rate)
    : AudioHandler(NodeType::kNodeTypeWaveShaper, node, sample_rate),
      processor_(std::make_unique<WaveShaperProcessor>(
          sample_rate,
          kDefaultNumberOfOutputChannels,
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames())) {
  AddInput();
  AddOutput(kDefaultNumberOfOutputChannels);

  Initialize();
}

void WaveShaperHandler::Process(uint32_t frames_to_process) {
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
}

void WaveShaperHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  if (!IsInitialized() || !Processor()) {
    return;
  }

  Processor()->ProcessOnlyAudioParams(frames_to_process);
}

void WaveShaperHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Initialize();

  AudioHandler::Initialize();
}

void WaveShaperHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  DCHECK(Processor());
  Processor()->Uninitialize();

  AudioHandler::Uninitialize();
}

void WaveShaperHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
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

bool WaveShaperHandler::RequiresTailProcessing() const {
  return processor_->RequiresTailProcessing();
}

double WaveShaperHandler::TailTime() const {
  return processor_->TailTime();
}

double WaveShaperHandler::LatencyTime() const {
  return processor_->LatencyTime();
}

void WaveShaperHandler::PullInputs(uint32_t frames_to_process) {
  // Render directly into output bus for in-place processing
  Input(0).Pull(Output(0).Bus(), frames_to_process);
}

unsigned WaveShaperHandler::NumberOfChannels() {
  return Output(0).NumberOfChannels();
}

WaveShaperProcessor* WaveShaperHandler::GetWaveShaperProcessor() {
  return static_cast<WaveShaperProcessor*>(Processor());
}

const WaveShaperProcessor* WaveShaperHandler::GetWaveShaperProcessor() const {
  return static_cast<const WaveShaperProcessor*>(Processor());
}

}  // namespace blink

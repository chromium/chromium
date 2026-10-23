// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_

#include <cstddef>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "media/webrtc/voice_isolation/spectral_voice_isolation_component.h"
#include "media/webrtc/voice_isolation/speech_mask_estimator.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"
#include "third_party/tflite/src/tensorflow/lite/interpreter.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

namespace tflite::resource {
class ResourceVariable;
}  // namespace tflite::resource

namespace media {

// Denoiser using a TfLite model internally. Ideally the provided model will
// remove all noise in the input signal preserving only human speech. The model
// has two outputs: the dereverberated output (noise and reverberation removed)
// and the denoising-only output (noise removed, reverberation kept).
// ProcessAudio() returns a mix of both that is mostly the dereverberated output
// and keeps a little of the room reverberation. The speech mask that
// SpeechMask() returns is estimated from the denoising-only output alone.
class COMPONENT_EXPORT(MEDIA_WEBRTC) TfLiteVoiceIsolation
    : public SpectralVoiceIsolationComponent {
 public:
  ~TfLiteVoiceIsolation() override;

  // This method takes a stack of two complex DFT coefficients of 20ms each with
  // a 50% overlap.
  void ProcessAudio(base::span<const float> input_dfts,
                    base::span<float> output_dfts) override;

  size_t FrameSize() const override;
  size_t FramesPerSecond() const override;
  void ClearBuffers() override;
  base::TimeDelta AlgorithmicDelay() const override;

  // 161 values, one per bin of the 16kHz DFTs, from DC to Nyquist. Estimated
  // over the window of the first input DFT of the call (so it lags the second
  // DFT by 10 ms); see SpectralVoiceIsolationComponent::SpeechMask() for the
  // boundary limitations at DC (bin 0), Nyquist (bin 160), and the top bins
  // near 8 kHz (bins 158..160).
  base::span<const float> SpeechMask() const override;

  // Creates a TfLiteVoiceIsolation running `model`, which needs to outlive the
  // returned instance.
  //
  // Model requirements. MaybeCreate() rejects a model that violates any of
  // them, and the audio service then stops using the model:
  // - All ops are supported by optimization_guide::TFLiteOpResolver
  //   (kInterpreterCreationFailed).
  // - One float32 input with 640 elements (two 16 kHz DFTs, see
  //   ProcessAudio()) and one float32 output with 1280 elements (per bin:
  //   dereverberated re, im, then denoising-only re, im). Both live in the
  //   interpreter arena: they are neither constant model data nor resized
  //   during Invoke() (kIncompatibleModel).
  // - The primary subgraph has static shapes (no dynamic tensors) and no
  //   control-flow ops other than CALL_ONCE (kIncompatibleModel).
  // - Invoke() succeeds on an all-zero input (kWarmupFailed). The output for
  //   that input, starting from the initial state, is subtracted from every
  //   frame as a bias.
  // - Recurrent state is kept in resource variables initialized to zero by
  //   CALL_ONCE ops of the primary subgraph, or in `is_variable` tensors, both
  //   of which are reset to zero. Each resource variable keeps its initial
  //   byte size, and every `is_variable` tensor is in a subgraph that runs
  //   (kIncompatibleModel).
  // - Zero lookahead (assumed, not checked): AlgorithmicDelay() is 0.
  static base::expected<std::unique_ptr<TfLiteVoiceIsolation>,
                        VoiceIsolationCreationResult>
  MaybeCreate(const tflite::FlatBufferModel* model);

 private:
  // A TFLite resource variable, where stateful models keep their recurrent
  // state, and its expected byte size.
  struct ResourceVariableState {
    raw_ptr<tflite::resource::ResourceVariable> variable;
    size_t bytes = 0;
  };

  TfLiteVoiceIsolation(std::unique_ptr<tflite::Interpreter> interpreter,
                       std::vector<float> bias);

  // Runs the model initialization subgraphs (the ones referenced by CallOnce
  // ops), verifies that they zero-initialize every resource variable, and
  // records the resource variables for `ResetState()`. Returns false if the
  // model has incompatible resource variables (e.g. uninitialized or non-zero
  // initial values, empty tensors, or a byte size that changes between
  // initialization and `Invoke()`).
  [[nodiscard]] bool InitResourceVariables();

  // Resets all model resource variables and `is_variable` tensors to zero in
  // place. Returns false if it cannot. Does not allocate memory, so it is safe
  // to call on the real-time audio thread.
  [[nodiscard]] bool ResetState();

  // Accumulates `elapsed` and logs the average and maximum model inference
  // execution time to UMA every 10 seconds of processed audio.
  void RecordInferenceExecutionTime(base::TimeDelta elapsed);

  std::unique_ptr<tflite::Interpreter> interpreter_;
  base::raw_span<float> input_tensor_span_;
  base::raw_span<float> output_tensor_span_;
  std::vector<float> bias_;
  std::vector<float> temp_output_;

  // Denoising-only output of the last model call, unpacked into two 16-byte
  // aligned DFTs for SpeechMaskEstimator::Estimate() and the output mix.
  base::AlignedHeapArray<float> denoising_only_dfts_;
  SpeechMaskEstimator speech_mask_estimator_;
  // Declared after `interpreter_`, which owns the resource variables.
  std::vector<ResourceVariableState> resource_variables_;

  // Execution time tracking for model inference UMA metrics.
  size_t inferences_in_log_period_ = 0;
  base::TimeDelta sum_inference_time_in_period_;
  base::TimeDelta max_inference_time_in_period_;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_TFLITE_VOICE_ISOLATION_H_

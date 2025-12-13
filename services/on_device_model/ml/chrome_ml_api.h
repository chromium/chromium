// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "services/on_device_model/ml/chrome_ml_types.h"
#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/webgpu.h"

// This header defines the public interface to the ChromeML shared library.

extern "C" {

typedef struct TfLiteDelegate TfLiteDelegate;

// A function used to handle fatal errors.
using ChromeMLFatalErrorFn = void (*)(const char* msg);

// A scheduling function used to run arbitrary async tasks. Given to
// CreateModelExecutor() and called into by ChromeML as needed. When called, the
// value of `context` is the same value given to CreateModelExecutor().
using ChromeMLScheduleFn = void (*)(uintptr_t context,
                                    std::function<void()>* task);

#if defined(_WIN32)
using PlatformFile = void*;
extern const PlatformFile kInvalidPlatformFile;
#else
using PlatformFile = int;
inline constexpr PlatformFile kInvalidPlatformFile = -1;
#endif

// Opaque handle to an instance of a ChromeML model.
using ChromeMLModel = uintptr_t;
// Opaque handle to an instance of a ChromeML session.
using ChromeMLSession = uintptr_t;
// Opaque handle to an object that allows canceling operations.
using ChromeMLCancel = uintptr_t;
// Opaque handle to an instance of a ChromeMLTS model.
using ChromeMLTSModel = uintptr_t;
// Opaque handle to an instance of a ChromeML ASR stream.
using ChromeMLASRStream = uintptr_t;
// Opaque handle to a constraint object.
using ChromeMLConstraint = uintptr_t;

// A contiguous byte span.
struct ChromeMLByteSpan {
  uint8_t* data;
  size_t size;
};

// Describes a ChromeML model's underlying tensors.
struct ChromeMLModelData {
  // File holding the weights data. The file will be owned by the inference
  // library and closed once weight loading is complete. kApuBackend provides
  // the `model_path` and not this field.
  PlatformFile weights_file;
  // A unique ID to identify `weights_file`s which point to the same data.
  // Matching `file_id` tells the backend that the data also matches.
  std::optional<uint32_t> file_id;

  // Files holding the weight cache. These files will be owned by the inference
  // library and closed upon model destruction.
  PlatformFile cache_file = kInvalidPlatformFile;
  PlatformFile encoder_cache_file = kInvalidPlatformFile;
  PlatformFile adapter_cache_file = kInvalidPlatformFile;

  // Null-terminated model path pointing to the model to use. Only kApuBackend
  // provides this field. Other backends provide model through the
  // `weights_file` field.
  const char* model_path = nullptr;

  // Null-terminated sentencepiece model path. kApuBackend models have a
  // separate sentencepiece model file and require this to be set. Other
  // backends have the sentencepiece model wrapped into the `weights_file`.
  const char* sentencepiece_model_path = nullptr;
};

// Describes a model to use with ChromeML.
struct ChromeMLModelDescriptor {
  // The backend to run this model.
  ml::ModelBackendType backend_type;

  // The model data to use.
  const ChromeMLModelData* model_data;

  // The maximum input+output tokens the model can handle.
  uint32_t max_tokens;

  // Output settings.
  float temperature;
  int top_k;

  // Speculative decoding
  int num_draft_tokens;

  // Packed TS data.
  const void* ts_data;
  size_t ts_size;
  const void* ts_spm_data;
  size_t ts_spm_size;
  size_t ts_dimension;

  const uint32_t* adaptation_ranks;
  size_t adaptation_ranks_size;

  bool prefer_texture_weights;
  bool enable_host_mapped_pointer;
  bool use_low_power;
  bool allow_fp16;

  ml::ModelPerformanceHint performance_hint;
};

// Describes an adaptation for a model.
struct ChromeMLAdaptationDescriptor {
  // The model data to use.
  const ChromeMLModelData* model_data;

  // The maximum input+output tokens the model can handle.
  // The default value 0 will be treated not set, and in that case the original
  // `max_tokens` set by the base model will be used.
  uint32_t max_tokens;

  // Parameters which control the output sampling.
  uint32_t top_k;
  float temperature;

  // Whether or not the mode will use speculative decoding.
  bool enable_speculative_decoding;

  // Whether this model will handle InputPieces containing images.
  bool enable_image_input;

  // Whether this model will handle InputPieces containing audio.
  bool enable_audio_input;
};

// A status value included with each output chunk.
enum class ChromeMLGenerateStatus {
  // Generation is still in progress and more outputs should be expected.
  kInProgress,

  // Generation either completed normally or was cancelled. This is the
  // last output.
  kComplete,
};
using ChromeMLExecutionStatus = ChromeMLGenerateStatus;

// Structure conveying sequential output from an in-progress generation.
struct ChromeMLGenerateOutput {
  // Status of this generation.
  ChromeMLGenerateStatus status;

  // Null-terminated text content for this output chunk, or null if there is no
  // new text output.
  const char* text;
};
using ChromeMLExecutionOutput = ChromeMLGenerateOutput;

struct ChromeMLTSModelDescriptor {
  ChromeMLByteSpan model;
  ChromeMLByteSpan sp_model;
  size_t dimensions;
};

// Status value indicating the result of ad hoc safety classification.
enum class ChromeMLSafetyResult {
  // Safety classification succeeded and the caller's output buffer has been
  // populated with the requested class scores.
  kOk,

  // The given ChromeMLModel does not have a valid safety classifier to use.
  kNoClassifier,

  // The caller's output buffer is insufficient to hold the complete set of
  // safety scores that would be output by the model's safety classifier.
  kInsufficientStorage,

  // Classification failed due to an internal model execution error.
  kModelExecutionFailure,
};

// Function provided from the library that will cancel the corresponding input
// and output when called. This is safe to call on any thread.
using ChromeMLCancelFn = std::function<void()>;

// Receives tokens an other information from a call to Generate(). This will
// be called on the internal thread holding the model. May be called multiple
// times; the final invocation will have output->status == kComplete. Note that
// `output` and any pointer fields therein are only valid through the extent of
// the function invocation and must not be retained by the callee.
using ChromeMLGenerateOutputFn =
    std::function<void(const ChromeMLGenerateOutput* output)>;
using ChromeMLExecutionOutputFn = ChromeMLGenerateOutputFn;

// Called with the number of tokens processed after a call to RunModel()
// which has the kSave ContextMode set. This will be called on the internal
// thread executing the model.
using ChromeMLContextSavedFn = std::function<void(int)>;

// Called with the number of tokens after a call to SizeInTokens().
// This will be called on the internal thread executing the model.
using ChromeMLSizeInTokensFn = std::function<void(int)>;

// Called with a probability score after a call to Score().
// This will be called on the internal thread executing the model.
using ChromeMLScoreFn = std::function<void(float)>;

// Called with a vector of probability scores after a call to
// GetProbabilitiesBlocking().
using ChromeMLGetProbabilitiesBlockingFn =
    std::function<void(const std::vector<float>&)>;

// Arguments to SessionAppend().
struct ChromeMLAppendOptions {
  // The content to append to the context.
  // Points to the first element of a list of `input_size`.
  const ml::InputPiece* input;
  // Number of pieces in input.
  size_t input_size;
  // The maximum number of tokens to add to the context.
  uint32_t max_tokens;
  // How to return the result on completion.
  const ChromeMLContextSavedFn* context_saved_fn;
};

// Arguments to SessionGenerate()
struct ChromeMLGenerateOptions {
  // Stop once this amount of tokens has been generated.
  uint32_t max_output_tokens;
  // A constraint to apply on the output. Ownership of this object is passed to
  // the callee.
  ChromeMLConstraint constraint;
  // How to return the generated tokens.
  const ChromeMLGenerateOutputFn* output_fn;
};

// DEPRECATED, migrating to Append/Generate.
struct ChromeMLExecuteOptions {
  int context_mode;
  uint32_t max_tokens;
  uint32_t max_output_tokens;
  const ChromeMLContextSavedFn* context_saved_fn;
  const ChromeMLExecutionOutputFn* execution_output_fn;
  // Optional adaptation ID for this request.
  uint32_t* adaptation_id;

  const ml::InputPiece* input;
  size_t input_size;

  // A constraint to apply on the output. Ownership of this object is passed to
  // the callee.
  ChromeMLConstraint constraint;
};

// Performance data filled out by GetEstimatedPerformance().
struct ChromeMLPerformanceInfo {
  float input_speed = 0.0f;
  float output_speed = 0.0f;
  bool is_integrated_gpu = false;
  uint64_t device_heap_size = 0;
  uint64_t max_buffer_size = 0;
};

// Structure needed to determine if the gpu is blockedlisted. Fields correspond
// to that in gpu::WebGpuBlockListParams.
struct GpuConfig {
  uint32_t vendor_id;
  uint32_t device_id;
  const char* architecture;
  const char* driver_description;
  // Corresponds to wgpu::AdapterType
  WGPUAdapterType adapter_type;
  // Corresponds to wgpu::BackendType
  WGPUBackendType backend_type;
};

// A set of capabilities that a model can have.
struct ChromeMLCapabilities {
  bool image_input = false;
  bool audio_input = false;
};

struct ChromeMLMetricsFns {
  // Logs an exact sample for the named metric.
  void (*RecordExactLinearHistogram)(const char* name,
                                     int sample,
                                     int exclusive_max);

  // Logs a sample for the named metric into one of a fixed number of buckets
  // spanning the specified range.
  void (*RecordCustomCountsHistogram)(const char* name,
                                      int sample,
                                      int min,
                                      int exclusive_max,
                                      size_t buckets);

  // Logs a sample for timings up to 3 minutes.
  void (*RecordMediumTimesHistogram)(const char* name, int64_t milliseconds);
};

// Represents a bitmask when generating a constraint.
struct ChromeMLConstraintMask {
  // Mask containing one bit per vocab token.
  const uint32_t* sample_mask;
  // Whether the sequence should stop.
  bool is_stop;
};

struct ChromeMLConstraintFns {
  // Delete the constraint.
  void (*Delete)(ChromeMLConstraint constraint);

  // Computes the mask to use for generating the next token.
  bool (*ComputeMask)(ChromeMLConstraint constraint,
                      ChromeMLConstraintMask& mask);

  // Commits the specified token to the constraint.
  bool (*CommitToken)(ChromeMLConstraint constraint, uint32_t token);

  // Returns true if the sequence cannot be extended any further.
  bool (*IsStopped)(ChromeMLConstraint constraint);

  // Gets the last error on this constraint or null for no error. The returned
  // string will be valid until the next call on this constraint.
  const char* (*GetError)(ChromeMLConstraint constraint);

  // Clones the constraint and associated state.
  ChromeMLConstraint (*Clone)(ChromeMLConstraint constraint);
};

// Tokenizes `bytes` and outputs into `output_tokens` at most
// `output_tokens_len`. Returns the total number of tokens in `bytes`.
using ChromeMLTokenizeFn = size_t (*)(const void* user_data,
                                      const uint8_t* bytes,
                                      size_t bytes_len,
                                      uint32_t* output_tokens,
                                      size_t output_tokens_len);

struct ChromeMLTokenizerParams {
  // The size of the token vocabulary from the LLM.
  uint32_t vocab_size;

  // The End of Sequence (EOS) token ID from the LLM.
  uint32_t eos_token_id;

  // An array of the lengths of the token strings (vocab_size elements).
  const uint32_t* token_lens;

  // A pointer to the token strings. The length of this is the sum of all
  // lengths from elements of token_lens.
  const uint8_t* token_bytes;

  // Instead of passing token_lens and token_bytes, this can be set to model's
  // tokenizer.json file content.
  const char* tokenizer_json_file_content;

  // Function for tokenizing a string. Will be passed `tokenize_user_data`.
  ChromeMLTokenizeFn tokenize_fn;
  const void* tokenize_user_data;
};

using ChromeMLGetTokenizerParamsFn =
    std::function<void(const ChromeMLTokenizerParams&)>;

// Precision used by the gpu delegate during inference.
enum class GpuDelegatePrecision { kFp16, kFp32 };

struct ChromeMLTSAPI {
  // Construct a text safety model.
  // Destroy the returned object by passing it to DestroyModel.
  ChromeMLTSModel (*CreateModel)(const ChromeMLTSModelDescriptor* descriptor);

  // Destroy a text safety model.
  void (*DestroyModel)(ChromeMLTSModel model);

  // Performs ad hoc safety classification on a chunk of text using the
  // classifier defined by `model`.
  //
  // On input, `scores` must point to an output buffer to receive the safety
  // class scores, and `num_scores` must point to the capacity of that buffer in
  // number of elements.
  //
  // On success this returns kOk on and `*num_scores` is set to the actual
  // number of score values written into the output buffer. This number is
  // guaranteed to be no larger than the input value of `*num_scores`.
  //
  // If this fails with kInsufficientStorage, no `scores` are populated and
  // `*num_scores` is set to the correct number scores the caller should expect.
  //
  // If `model` does not define a safety classifier, this returns kNoClassifier.
  ChromeMLSafetyResult (*ClassifyTextSafety)(ChromeMLTSModel model,
                                             const char* text,
                                             float* scores,
                                             size_t* num_scores);
};

struct ChromeMLASRStreamOutputTranscript {
  const char* transcript;
  bool is_final;
};
using ChromeMLASRStreamOutput = std::vector<ChromeMLASRStreamOutputTranscript>;

using ChromeMLASRStreamOutputFn =
    std::function<void(const ChromeMLASRStreamOutput&)>;

struct ChromeMLASRStreamOptions {
  uint32_t sample_rate_hz;
  // Function to call with transcribed audio.
  const ChromeMLASRStreamOutputFn* output_fn;
};

struct ChromeMLASRAPI {
  // Create a new ASR stream on an existing ML session.
  ChromeMLASRStream (*CreateStream)(ChromeMLSession session,
                                    const ChromeMLASRStreamOptions* options);
  // Add an audio chunk to the ASR session.
  void (*AddAudioChunk)(ChromeMLASRStream stream,
                        ml::AudioBuffer* audio_buffer);
  // Note: This does not destroy the parent ChromeMLSession.
  void (*DestroyStream)(ChromeMLASRStream stream);
};

// IMPORTANT: All functions that call ChromeMLAPI should be annotated with
// DISABLE_CFI_DLSYM.

// Table of C API functions defined within the library.
struct ChromeMLAPI {
  // Initializes the Dawn proc table. This must be called before any other
  // functions.
  void (*InitDawnProcs)(const DawnProcTable& procs);

  // Sets functions which can be used to log metrics from within the library.
  void (*SetMetricsFns)(const ChromeMLMetricsFns* fns);

  // Sets an error handling function for fatal errors in the GPU. See also
  // SetFatalErrorNonGpuFn.
  void (*SetFatalErrorFn)(ChromeMLFatalErrorFn error_fn);

  // Performs ad hoc safety classification on a chunk of text using the
  // classifier defined by `model`.
  //
  // On input, `scores` must point to an output buffer to receive the safety
  // class scores, and `num_scores` must point to the capacity of that buffer in
  // number of elements.
  //
  // On success this returns kOk on and `*num_scores` is set to the actual
  // number of score values written into the output buffer. This number is
  // guaranteed to be no larger than the input value of `*num_scores`.
  //
  // If this fails with kInsufficientStorage, no `scores` are populated and
  // `*num_scores` is set to the correct number scores the caller should expect.
  //
  // If `model` does not define a safety classifier, this returns kNoClassifier.
  ChromeMLSafetyResult (*ClassifyTextSafety)(ChromeMLModel model,
                                             const char* text,
                                             float* scores,
                                             size_t* num_scores);

  // Destroys a model that was created by SessionCreateModel().
  void (*DestroyModel)(ChromeMLModel model);

  // Estimates the tokens per second this device will be able to achieve when
  // running a typical model.
  bool (*GetEstimatedPerformance)(ChromeMLPerformanceInfo* performance_info);

  // Query the GPU adapter used.
  // Synchronously calls `adapter_callback_fn` with a non-owning pointer to the
  // adapter. Returns false if there was an error getting an adapter at all; the
  // callback is not called. It is not safe to save reference to this adapter as
  // it is allocated in another dll. Use of the adapter must only be scoped to
  // the duration of `adapter_callback_fn`.
  bool (*QueryGPUAdapter)(void (*adapter_callback_fn)(WGPUAdapter adapter,
                                                      void* userdata),
                          void* userdata);

  // Gets the model capabilities for the model pointed to by `model_data`.
  bool (*GetCapabilities)(PlatformFile file,
                          ChromeMLCapabilities& capabilities);

  // Same as SetFatalErrorFn(), but for fatal errors that occur outside of the
  // gpu.
  void (*SetFatalErrorNonGpuFn)(ChromeMLFatalErrorFn error_fn);

  // Creates a new ChromeML model instance as described by `descriptor`. The
  // returned object can be destroyed by passing it to DestroyModel(). `context`
  // is forwarded to any invocations of `schedule` or `token_output` made by
  // this model.
  ChromeMLModel (*SessionCreateModel)(const ChromeMLModelDescriptor* descriptor,
                                      uintptr_t context,
                                      ChromeMLScheduleFn schedule);

  // Appends input to the Session's context.
  // May be cancelled by calling CancelExecuteModel on cancel.
  bool (*SessionAppend)(ChromeMLSession session,
                        const ChromeMLAppendOptions* options,
                        ChromeMLCancel cancel);

  // Requests output to be generated by the model, appending it in the context.
  // May be cancelled by calling CancelExecuteModel on cancel.
  bool (*SessionGenerate)(ChromeMLSession session,
                          const ChromeMLGenerateOptions* options,
                          ChromeMLCancel cancel);

  // DEPRECATED, migrating to Append/Generate.
  // Executes a model given the input `options.input`. Results are fed
  // incrementally to `options.execution_output_fn`. Execution may be cancelled
  // by calling CancelExecuteModel on `cancel`.
  bool (*SessionExecuteModel)(ChromeMLSession session,
                              ChromeMLModel model,
                              const ChromeMLExecuteOptions* options,
                              ChromeMLCancel cancel);

  // Get the size of the given text in tokens.
  void (*SessionSizeInTokens)(ChromeMLSession session,
                              const std::string& text,
                              const ChromeMLSizeInTokensFn& fn);
  void (*SessionSizeInTokensInputPiece)(ChromeMLSession session,
                                        ChromeMLModel model,
                                        const ml::InputPiece* input,
                                        size_t input_size,
                                        const ChromeMLSizeInTokensFn& fn);

  // Scores the first token of the given text.
  void (*SessionScore)(ChromeMLSession session,
                       const std::string& text,
                       const ChromeMLScoreFn& fn);

  // Get the probabilities of a batch of tokens.
  // Note that this is a blocking call, and mainly used for testing purpose.
  void (*SessionGetProbabilitiesBlocking)(
      ChromeMLSession session,
      const std::string& input,
      const ChromeMLGetProbabilitiesBlockingFn& fn);

  // Create a new session in the model, optionally loading adaptation data.
  ChromeMLSession (*CreateSession)(
      ChromeMLModel model,
      const ChromeMLAdaptationDescriptor* descriptor);

  // Clone an existing session.
  ChromeMLSession (*CloneSession)(ChromeMLSession session);

  // Destroy a session.
  void (*DestroySession)(ChromeMLSession session);

  ChromeMLCancel (*CreateCancel)();
  void (*DestroyCancel)(ChromeMLCancel cancel);
  void (*CancelExecuteModel)(ChromeMLCancel cancel);

  // Sets constraint functions to be used in the shared library.
  void (*SetConstraintFns)(const ChromeMLConstraintFns* fns);

  // Gets parameters needed to construct a tokenizer.
  bool (*GetTokenizerParams)(ChromeMLModel model,
                             const ChromeMLGetTokenizerParamsFn& fn);

  // Creates a new TFLite delegate using the GPU inference engine.
  TfLiteDelegate* (*CreateGpuDelegate)();

  TfLiteDelegate* (*CreateGpuDelegateWithPrecision)(
      GpuDelegatePrecision precision);

  // Destroys the TFLite delegate created by `CreateDelegate()` call.
  void (*DestroyGpuDelegate)(TfLiteDelegate* delegate);

  ChromeMLTSAPI ts_api;
  ChromeMLASRAPI asr_api;
};

// Signature of the GetChromeMLAPI() function which the shared library exports.
using ChromeMLAPIGetter = const ChromeMLAPI* (*)(bool enable_litert_lm);

}  // extern "C"

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

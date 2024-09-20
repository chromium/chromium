// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

#include <cstdint>
#include <functional>
#include <string>

#include "services/on_device_model/ml/chrome_ml_types.h"
#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/webgpu.h"

// This header defines the public interface to the ChromeML shared library.

extern "C" {

// A function used to handle fatal errors.
using ChromeMLFatalErrorFn = void (*)(const char* msg);

// A scheduling function used to run arbitrary async tasks. Given to
// CreateModelExecutor() and called into by ChromeML as needed. When called, the
// value of `context` is the same value given to CreateModelExecutor().
using ChromeMLScheduleFn = void (*)(uintptr_t context,
                                    std::function<void()>* task);

#if defined(_WIN32)
using PlatformFile = void*;
#else
using PlatformFile = int;
#endif

// Opaque handle to an instance of a ChromeML model.
using ChromeMLModel = uintptr_t;
// Opaque handle to an instance of a ChromeML session.
using ChromeMLSession = uintptr_t;
// Opaque handle to an object that allows canceling operations.
using ChromeMLCancel = uintptr_t;
// Opaque handle to an instance of a ChromeMLTS model.
using ChromeMLTSModel = uintptr_t;
// Opaque handle to a video-frame-specific ML inference engine.
using ChromeMLInferenceEngine = uintptr_t;

// Type of the backend to run the model.
enum ModelBackendType {
  // The default WebGPU backend.
  kGpuBackend = 0,
  // The APU accelerator backend. Only available on devices with APU, and need
  // special APU model files.
  kApuBackend = 1,
};

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
  ModelBackendType backend_type;

  // The model data to use.
  const ChromeMLModelData* model_data;

  // The maximum input+output tokens the model can handle.
  uint32_t max_tokens;

  // Output settings.
  float temperature;
  int top_k;

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
};

// Describes an adaptation for a model.
struct ChromeMLAdaptationDescriptor {
  // The model data to use.
  const ChromeMLModelData* model_data;
};

// A status value included with each output chunk.
enum class ChromeMLExecutionStatus {
  // Model execution is still in progress and more outputs should be expected.
  kInProgress,

  // Model execution either completed normally or was cancelled. This is the
  // last output.
  kComplete,
};

// Structure conveying sequential output from an in-progress model execution.
struct ChromeMLExecutionOutput {
  // Status of this model execution.
  ChromeMLExecutionStatus status;

  // Null-terminated text content for this output chunk, or null if there is no
  // new text output.
  const char* text;
};

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

// Receives tokens an other information from a call to ExecuteModel(). This will
// be called on the internal thread executing the model. May be multiple times,
// and the final invocation will be indicated by the `status` field within
// `output`. Note that `output` and any pointer fields therein are only valid
// through the extent of the function invocation and must not be retained by
// the callee.
using ChromeMLExecutionOutputFn =
    std::function<void(const ChromeMLExecutionOutput* output)>;

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

struct ChromeMLExecuteOptions {
  const char* prompt;
  int context_mode;
  uint32_t max_tokens;
  uint32_t token_offset;
  uint32_t max_output_tokens;
  const ChromeMLContextSavedFn* context_saved_fn;
  const ChromeMLExecutionOutputFn* execution_output_fn;
  // Optional adaptation ID for this request.
  uint32_t* adaptation_id;
  uint32_t top_k;
  float temperature;

  const ml::InputPiece* input;
  size_t input_size;
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
};

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

  // Executes a model given the input `options.prompt`. Results are fed
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

  // Create new instance of ML inference engine, using the passed in `device`.
  // `model_blob` should contain a binary blob of a TFLite model (read from
  // .tflite file). `model_blob_size` is the size in bytes of `model_blob`. On
  // failure, will return `0`.
  ChromeMLInferenceEngine (*CreateInferenceEngine)(WGPUAdapterInfo adapter_info,
                                                   WGPUDevice device,
                                                   const char* model_blob,
                                                   size_t model_blob_size);

  // Runs inference on `source`, producing results into `destination`. `engine`
  // must have been obtained from `CreateInferenceEngine()` call.
  bool (*RunInference)(ChromeMLInferenceEngine engine,
                       WGPUTexture source,
                       WGPUTexture destination);

  // Cleans up the instance of ML inference engine returned from
  // `CreateInferenceEngine()` call. It is invalid to use `engine` for inference
  // after this call.
  void (*DestroyInferenceEngine)(ChromeMLInferenceEngine engine);

  ChromeMLTSAPI ts_api;
};

// Signature of the GetChromeMLAPI() function which the shared library exports.
using ChromeMLAPIGetter = const ChromeMLAPI* (*)();

}  // extern "C"

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

#include <cstdint>
#include <functional>
#include <vector>

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

enum ContextMode {
  kNone = 0,
  kReset = 1 << 0,
  kSave = 1 << 1,
  kIgnoreContext = 1 << 2,
};

// Opaque handle to an instance of a ChromeML model.
using ChromeMLModel = uintptr_t;

// Function called to release resources.
using ChromeMLDisposeFn = std::function<void()>;

// Describes a ChromeML model's underlying tensors.
struct ChromeMLModelData {
  // Points to a serialized description of the model's tensors.
  const void* model_proto_data;

  // The size in bytes of the serialized proto at `model_proto_data`.
  size_t model_proto_size;

  // Called when the model_proto data is no longer needed.
  const ChromeMLDisposeFn* model_proto_dispose;

  // Points to raw tensor weight data, indexed by fields encoded in the above
  // proto. This memory must be mutable.
  void* weights_data;

  // The size in bytes of the data at `weights_data`.
  size_t weights_size;

  // Called when the weights data is no longer needed.
  const ChromeMLDisposeFn* weights_dispose;
};

// Describes a model to use with ChromeML.
struct ChromeMLModelDescriptor {
  // Points to a serialized sentencepiece.ModelProto proto.
  const void* sentencepiece_model_proto_data;

  // The size in bytes of the serialized proto at `sentencepiece_model_data`.
  size_t sentencepiece_model_proto_size;

  // Called when the sentencepiece_model_proto data is no longer needed.
  const ChromeMLDisposeFn* sentencepiece_model_proto_dispose;

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

  // Optional TS scores for the full output so far, up to and including this
  // chunk. Only included as specified by `score_ts_interval` in
  // ChromeMLExecuteOptions.
  //
  // If no new scores are provided for this output, this field is null and
  // `num_ts_scores` is zero.
  float* ts_scores;
  size_t num_ts_scores;
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

// Receives tokens from a call to RunModel(). This will be called on the
// internal thread executing the model. If no completion callback is provided to
// ExecuteModel(), this function will be invoked with std::nullopt to signify
// that model execution is complete.
//
// DEPRECATED: Use a ChromeMLExecutionOutputFn instead.
using ChromeMLOutputFn = std::function<void(const std::optional<std::string>&)>;

// Receives periodic updates to TS scores, per `score_ts_interval` set in
// ChromeMLExecuteOptions.
//
// DEPRECATED: Use a ChromeMLExecutionOutputFn instead.
using ChromeMLScoreTSFn = std::function<void(const std::vector<float>&)>;

// Called with the number of tokens processed after a call to RunModel()
// which has the kSave ContextMode set. This will be called on the internal
// thread executing the model.
using ChromeMLContextSavedFn = std::function<void(int)>;

// Conveys details regarding a completed model execution.
struct ChromeMLExecutionResult {
  // If true, all prior output received for this model execution is effectively
  // retracted by the library and should be discarded by the client.
  //
  // DEPRECATED: Clients should ignore this field. It will be deleted.
  bool retracted;
};

// Called when a model has finished executing. No other functions given to
// ExecuteModel() will be invoked after this.
//
// DEPRECATED: Use a ChromeMLExecutionOutputFn instead.
using ChromeMLCompletionFn =
    std::function<void(const ChromeMLExecutionResult&)>;

struct ChromeMLExecuteOptions {
  const char* prompt;
  int context_mode;
  uint32_t max_tokens;
  uint32_t token_offset;
  uint32_t max_output_tokens;
  int32_t score_ts_interval;
  const ChromeMLOutputFn* output_fn;
  const ChromeMLScoreTSFn* score_ts_fn;
  const ChromeMLContextSavedFn* context_saved_fn;
  const ChromeMLCompletionFn* completion_fn;
  const ChromeMLExecutionOutputFn* execution_output_fn;
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
  void (*SetFatalErrorFn)(ChromeMLFatalErrorFn error_fn) = nullptr;

  // Creates a new ChromeML model instance as described by `model`. The returned
  // object can be destroyed by passing it to DestroyModel(). `context` is
  // forwarded to any invocations of `schedule` or `token_output` made by this
  // model.
  ChromeMLModel (*CreateModel)(const ChromeMLModelDescriptor* descriptor,
                               uintptr_t context,
                               ChromeMLScheduleFn schedule);

  // Executes a model given the input `prompt`. Results are fed incrementally
  // to the model's given ChromeMLOutputFn.
  bool (*ExecuteModel)(ChromeMLModel model,
                       const ChromeMLExecuteOptions* options,
                       ChromeMLCancelFn* cancel_fn);

  // Destroys a model that was created by CreateModel().
  void (*DestroyModel)(ChromeMLModel model);

  // Estimates the tokens per second this device will be able to achieve when
  // running a typical model.
  bool (*GetEstimatedPerformance)(ChromeMLPerformanceInfo* performance_info);

  // Returns the GpuConfig in `config`. Returns true on success, false if there
  // was an error calculating it.
  bool (*GetGpuConfig)(GpuConfig& config);

  // Same as SetFatalErrorFn(), but for fatal errors that occur outside of the
  // gpu.
  void (*SetFatalErrorNonGpuFn)(ChromeMLFatalErrorFn error_fn) = nullptr;
};

// Signature of the GetChromeMLAPI() function which the shared library exports.
using ChromeMLAPIGetter = const ChromeMLAPI* (*)();

}  // extern "C"

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

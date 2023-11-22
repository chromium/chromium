// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_
#define SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

#include <cstdint>
#include <functional>

#include "third_party/dawn/include/dawn/dawn_proc_table.h"
#include "third_party/dawn/include/dawn/webgpu.h"

// This header defines the public interface to the ChromeML shared library.

extern "C" {

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

// Describes a ChromeML model's underlying tensors.
struct ChromeMLModelData {
  // Points to a serialized description of the model's tensors.
  const void* model_proto_data;

  // The size in bytes of the serialized proto at `model_proto_data`.
  size_t model_proto_size;

  // Points to raw tensor weight data, indexed by fields encoded in the above
  // proto. This memory must be mutable.
  void* weights_data;

  // The size in bytes of the data at `weights_data`.
  size_t weights_size;
};

// Describes a model to use with ChromeML.
struct ChromeMLModelDescriptor {
  // Points to a serialized sentencepiece.ModelProto proto.
  const void* sentencepiece_model_proto_data;

  // The size in bytes of the serialized proto at `sentencepiece_model_data`.
  size_t sentencepiece_model_proto_size;

  // The model data to use.
  const ChromeMLModelData* model_data;

  // The maximum input+output tokens the model can handle.
  uint32_t max_tokens;
};

// Function provided from the library that will cancel the corresponding input
// and output when called. This is safe to call on any thread.
using ChromeMLCancelFn = std::function<void()>;

// Receives tokens from a call to RunModel(). If the output is complete or
// there is an error the token will be std::nullopt. This will be called on the
// internal thread executing the model.
using ChromeMLOutputFn = std::function<void(const std::optional<std::string>&)>;

// Called with the number of tokens processed after a call to RunModel()
// which has the kSave ContextMode set. This will be called on the internal
// thread executing the model.
using ChromeMLContextSavedFn = std::function<void(int)>;

struct ChromeMLExecuteOptions {
  const char* prompt = nullptr;
  int context_mode = ContextMode::kNone;
  uint32_t max_tokens = 0;
  uint32_t token_offset = 0;
  const ChromeMLOutputFn* output_fn = nullptr;
  const ChromeMLContextSavedFn* context_saved_fn = nullptr;
};

// Performance data filled out by GetEstimatedPerformance().
struct ChromeMLPerformanceInfo {
  float input_speed = 0.0f;
  float output_speed = 0.0f;
  bool is_integrated_gpu = false;
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

// Table of C API functions defined within the library.
struct ChromeMLAPI {
  // Initializes the Dawn proc table. This must be called before any other
  // functions.
  void (*InitDawnProcs)(const DawnProcTable& procs);

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
};

// Signature of the GetChromeMLAPI() function which the shared library exports.
using ChromeMLAPIGetter = const ChromeMLAPI* (*)();

}  // extern "C"

#endif  // SERVICES_ON_DEVICE_MODEL_ML_CHROME_ML_API_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ML_COMPILATION_DELEGATE_IE_H_
#define SERVICES_ML_COMPILATION_DELEGATE_IE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ml/common.h"
#include "services/ml/compilation_impl.h"
#include "services/ml/public/mojom/model.mojom.h"

namespace InferenceEngine {
class CNNNetwork;
class Blob;
namespace Builder {
class Network;
}
}  // namespace InferenceEngine

namespace ml {

static float asfloat(uint32_t v) {
  union {
    float f;
    std::uint32_t u;
  } converter = {0};
  converter.u = v;
  return converter.f;
}

static short f32tof16(float x) {
  static float min16 = asfloat((127 - 14) << 23);

  static float max16 = asfloat(((127 + 15) << 23) | 0x007FE000);
  static uint32_t max16f16 = ((15 + 15) << 10) | 0x3FF;

  static constexpr std::uint32_t EXP_MASK_F32 = 0x7F800000U;

  union {
    float f;
    uint32_t u;
  } v = {0};
  v.f = x;

  uint32_t s = (v.u >> 16) & 0x8000;

  v.u &= 0x7FFFFFFF;

  if ((v.u & EXP_MASK_F32) == EXP_MASK_F32) {
    if (v.u & 0x007FFFFF) {
      return static_cast<short>(s | (v.u >> (23 - 10)) | 0x0200);
    } else {
      return static_cast<short>(s | (v.u >> (23 - 10)));
    }
  }

  float halfULP = asfloat(v.u & EXP_MASK_F32) * asfloat((127 - 11) << 23);
  v.f += halfULP;

  if (v.f < min16 * 0.5f) {
    return static_cast<short>(s);
  }

  if (v.f < min16) {
    return static_cast<short>(s | (1 << 10));
  }

  if (v.f >= max16) {
    return static_cast<short>(max16f16 | s);
  }

  v.u -= ((127 - 15) << 23);

  v.u >>= (23 - 10);

  return static_cast<short>(v.u | s);
}

template <typename T>
int32_t Reorder(T* dst,
                const float* src,
                std::vector<uint32_t>& dims,
                bool nhwc_to_nchw = true) {
  if (!(std::is_same<T, float>::value || std::is_same<T, int16_t>::value ||
        std::is_same<T, int32_t>::value)) {
    LOG(ERROR) << "Data type is not supported";
    return mojom::BAD_DATA;
  }
  if (dims.size() == 1 || dims.size() == 2) {
    size_t size = product(dims);
    if (std::is_same<T, float>::value || std::is_same<T, int32_t>::value) {
      const size_t buffer_length = size * sizeof(T);
      memcpy(static_cast<void*>(dst), static_cast<const void*>(src),
             buffer_length);
    } else if (std::is_same<T, int16_t>::value) {
      for (size_t i = 0; i < size; ++i) {
        dst[i] = f32tof16(src[i]);
      }
    }
  } else if (dims.size() == 3 || dims.size() == 4) {
    // dims is in NHWC
    const bool rank3 = dims.size() == 3;
    const uint32_t batches = rank3 ? 1 : dims[0];
    const uint32_t channels = rank3 ? dims[2] : dims[3];
    const uint32_t height = rank3 ? dims[0] : dims[1];
    const uint32_t width = rank3 ? dims[1] : dims[2];

    for (uint32_t b = 0; b < batches; ++b) {
      for (uint32_t c = 0; c < channels; ++c) {
        for (uint32_t y = 0; y < height; ++y) {
          for (uint32_t x = 0; x < width; ++x) {
            uint32_t dst_index, src_index;
            if (nhwc_to_nchw) {
              dst_index = b * channels * height * width + c * height * width +
                          y * width + x;
              src_index = b * height * width * channels + y * width * channels +
                          x * channels + c;
            } else {
              dst_index = b * height * width * channels + y * width * channels +
                          x * channels + c;
              src_index = b * channels * height * width + c * height * width +
                          y * width + x;
            }
            if (std::is_same<T, float>::value ||
                std::is_same<T, int32_t>::value) {
              dst[dst_index] = src[src_index];
            } else if (std::is_same<T, int16_t>::value) {
              dst[dst_index] = f32tof16(src[src_index]);
            }
          }
        }
      }
    }
  } else {
    LOG(ERROR) << "Tensor rank " << dims.size() << " is not supproted";
    return mojom::BAD_DATA;
  }
  return mojom::NOT_ERROR;
}

bool GNADevice();

class CompilationDelegateIe : public CompilationDelegate {
 public:
  explicit CompilationDelegateIe(const CompilationImpl*);
  ~CompilationDelegateIe() override;

  int32_t Compile() override;
  int32_t CreateExecution(std::unique_ptr<mojom::Execution>& execution,
                          mojom::ExecutionInitParamsPtr params) override;

 private:
  friend class ExecutionImplIe;
  static int32_t GetDims(const std::vector<uint32_t>&, std::vector<size_t>&);
  int32_t Init();
  int32_t BuildNetwork();
  int32_t CreateBlob(uint32_t index,
                     std::shared_ptr<InferenceEngine::Blob>& blob);
  int32_t AddInput(uint32_t index);
  int32_t AddOutput(uint32_t index);
  int32_t AddConstant(uint32_t index);
  int32_t AddActivationByFusedCode(int32_t fuse_code,
                                   size_t input_layer,
                                   const std::string& name,
                                   size_t& activiation_layer_id);
  int32_t AddElementwise(const mojom::OperationPtr& operation);
  int32_t AddConvolution(const mojom::OperationPtr& operation);
  int32_t AddPooling(const mojom::OperationPtr& operation);
  int32_t AddSoftmax(const mojom::OperationPtr& operation);
  int32_t AddReshape(const mojom::OperationPtr& operation);
  int32_t AddConcatenation(const mojom::OperationPtr& operation);
  int32_t AddFullyConnected(const mojom::OperationPtr& operation);
  int32_t AddResizeBilinear(const mojom::OperationPtr& operation);
  int32_t AddSigmoid(const mojom::OperationPtr& operation);
  int32_t AddArgmax(const mojom::OperationPtr& operation);

 private:
  const CompilationImpl* compilation_;

  std::unique_ptr<InferenceEngine::Builder::Network> builder_;
  std::unique_ptr<InferenceEngine::CNNNetwork> network_;

  std::map<uint32_t, size_t> layer_id_map_;

  DISALLOW_COPY_AND_ASSIGN(CompilationDelegateIe);
};

}  // namespace ml

#endif  // SERVICES_ML_COMPILATION_DELEGATE_IE_H_
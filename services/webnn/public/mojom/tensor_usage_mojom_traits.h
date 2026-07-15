// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_TENSOR_USAGE_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_TENSOR_USAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/ml_tensor_usage.h"
#include "services/webnn/public/mojom/webnn_tensor.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::TensorUsageDataView, webnn::MLTensorUsage> {
  static bool web_gpu_interop(const webnn::MLTensorUsage& usage) {
    return usage.Has(webnn::MLTensorUsageFlags::kWebGpuInterop);
  }

  static bool write(const webnn::MLTensorUsage& usage) {
    return usage.Has(webnn::MLTensorUsageFlags::kWrite);
  }

  static bool read(const webnn::MLTensorUsage& usage) {
    return usage.Has(webnn::MLTensorUsageFlags::kRead);
  }

  static bool Read(webnn::mojom::TensorUsageDataView data,
                   webnn::MLTensorUsage* out) {
    out->Clear();

    if (data.web_gpu_interop()) {
      out->Put(webnn::MLTensorUsageFlags::kWebGpuInterop);
    }

    if (data.read()) {
      out->Put(webnn::MLTensorUsageFlags::kRead);
    }

    if (data.write()) {
      out->Put(webnn::MLTensorUsageFlags::kWrite);
    }

    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_TENSOR_USAGE_MOJOM_TRAITS_H_

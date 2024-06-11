// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_MOJOM_BUFFER_USAGE_MOJOM_TRAITS_H_
#define SERVICES_WEBNN_PUBLIC_MOJOM_BUFFER_USAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/webnn/public/cpp/ml_buffer_usage.h"
#include "services/webnn/public/mojom/webnn_buffer.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<webnn::mojom::BufferUsageDataView, webnn::MLBufferUsage> {
  static bool web_gpu_interop(const webnn::MLBufferUsage& usage) {
    return usage.Has(webnn::MLBufferUsageFlags::kWebGpuInterop);
  }

  static bool Read(webnn::mojom::BufferUsageDataView data,
                   webnn::MLBufferUsage* out) {
    out->Clear();

    if (data.web_gpu_interop()) {
      out->Put(webnn::MLBufferUsageFlags::kWebGpuInterop);
    }
    return true;
  }
};

}  // namespace mojo

#endif  // SERVICES_WEBNN_PUBLIC_MOJOM_BUFFER_USAGE_MOJOM_TRAITS_H_

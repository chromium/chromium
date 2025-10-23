// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_
#define SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_

#include "services/webnn/ort/device_allocator.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/cpp/operand_descriptor.h"

namespace webnn::ort {

// The internal contents of an MLTensor. Access should be managed by wrapping in
// a `QueueableResourceState`.
class BufferContentOrt {
 public:
  explicit BufferContentOrt(const OperandDescriptor& descriptor,
                            scoped_refptr<DeviceAllocator> device_allocator);

  BufferContentOrt(const BufferContentOrt&) = delete;
  BufferContentOrt& operator=(const BufferContentOrt&) = delete;

  ~BufferContentOrt();

  OrtValue* tensor() const { return tensor_.get(); }
  base::span<uint8_t> AsSpan() const;

 private:
  // The device allocator used for device tensor creation. May be nullptr if
  // device tensor is not supported.
  // If the device allocator is present, the tensor is allocated by the device
  // allocator, and its destruction depends on the allocator remaining valid.
  // Therefore, the device allocator must be referenced by `BufferContentOrt`
  // and declared before `tensor_` to ensure correct destruction order to avoid
  // use-after-free errors.
  scoped_refptr<DeviceAllocator> device_allocator_;
  ScopedOrtValue tensor_;
  size_t size_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_BUFFER_CONTENT_ORT_H_

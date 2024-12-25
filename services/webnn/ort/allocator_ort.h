// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ALLOCATOR_ORT_H_
#define SERVICES_WEBNN_ORT_ALLOCATOR_ORT_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "third_party/microsoft_dxheaders/include/onnxruntime_c_api.h"

namespace webnn::ort {

// TODO: Figure out if allocator is really thread safe.
class COMPONENT_EXPORT(WEBNN_SERVICE) AllocatorOrt final
    : public base::RefCountedThreadSafe<AllocatorOrt> {
 public:
  // TODO: Now it's for CPU only, need to support other devices.
  static scoped_refptr<AllocatorOrt> GetInstance();

  const OrtEnv* env() const { return env_.get(); }
  const OrtMemoryInfo* memory_info() const { return memory_info_.get(); }
  OrtAllocator* allocator() const { return allocator_.get(); }

 private:
  friend class base::RefCountedThreadSafe<AllocatorOrt>;
  AllocatorOrt(OrtEnv* env, OrtMemoryInfo* info, OrtAllocator* allocator);
  ~AllocatorOrt();

  static scoped_refptr<AllocatorOrt> Create();

  raw_ptr<OrtEnv> env_;
  raw_ptr<OrtMemoryInfo> memory_info_;
  raw_ptr<OrtAllocator> allocator_;

  static AllocatorOrt* instance_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ALLOCATOR_ORT_H_

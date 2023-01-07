// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_BUFFER_DEV_H_
#define PPAPI_CPP_DEV_BUFFER_DEV_H_

#include <stdint.h>

#include "ppapi/cpp/resource.h"

namespace pp {

class InstanceHandle;

class Buffer_Dev : public Resource {
 public:
  // Creates an is_null() Buffer object.
  Buffer_Dev();
  Buffer_Dev(const Buffer_Dev& other);
  explicit Buffer_Dev(PP_Resource resource);

  // Creates & Maps a new Buffer in the browser with the given size. The
  // resulting object will be is_null() if either Create() or Map() fails.
  Buffer_Dev(const InstanceHandle& instance, uint32_t size);

  // Constructor used when the buffer resource already has a reference count
  // assigned. No additional reference is taken.
  Buffer_Dev(PassRef, PP_Resource resource);

  // Unmap the underlying shared memory.
  virtual ~Buffer_Dev();

  Buffer_Dev& operator=(const Buffer_Dev& rhs);

  uint32_t size() const { return size_; }
  void* data() const { return data_; }

 private:
  void Init();

  void* data_;
  uint32_t size_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_BUFFER_DEV_H_

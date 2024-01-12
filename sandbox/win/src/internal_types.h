// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_SRC_INTERNAL_TYPES_H_
#define SANDBOX_WIN_SRC_INTERNAL_TYPES_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"

namespace sandbox {

const wchar_t kNtdllName[] = L"ntdll.dll";
const wchar_t kKerneldllName[] = L"kernel32.dll";

// Defines the supported C++ types encoding to numeric id. Like a simplified
// RTTI. Note that true C++ RTTI will not work because the types are not
// polymorphic anyway.
enum ArgType {
  INVALID_TYPE = 0,
  WCHAR_TYPE,
  UINT32_TYPE,
  VOIDPTR_TYPE,
  INOUTPTR_TYPE,
  LAST_TYPE
};

// Encapsulates a pointer to a buffer and the size of the buffer.
class CountedBuffer {
 public:
  CountedBuffer(void* buffer, uint32_t size) : size_(size), buffer_(buffer) {}

  uint32_t Size() const { return size_; }

  void* Buffer() const { return buffer_; }

 private:
  uint32_t size_;
  raw_ptr<void> buffer_;
};

// Helper class to convert void-pointer packed ints for both
// 32 and 64 bit builds. This construct is non-portable.
class IPCInt {
 public:
  explicit IPCInt(void* buffer) { buffer_.vp = buffer; }

  explicit IPCInt(uint32_t i32) {
    buffer_.vp = nullptr;
    buffer_.i32 = i32;
  }

  uint32_t As32Bit() const { return buffer_.i32; }

  void* AsVoidPtr() const { return buffer_.vp; }

 private:
  union U {
    // This field is not a raw_ptr<> because it was filtered by the rewriter
    // for: #union
    RAW_PTR_EXCLUSION void* vp;
    uint32_t i32;
  } buffer_;
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_INTERNAL_TYPES_H_

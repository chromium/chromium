// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_BUFFER_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_BUFFER_ANDROID_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "services/tracing/public/cpp/perfetto/java_heap_profiler/hprof_data_type_android.h"

namespace tracing {

// Helper class that has methods to help parse the hprof file data passed in.
// Works by accessing byte one at a time through data_[data_position_] while
// also incrementing |data_position_| after reading a byte.
class COMPONENT_EXPORT(TRACING_CPP) HprofBuffer {
 public:
  HprofBuffer(const unsigned char* data, size_t size);
  HprofBuffer(const HprofBuffer&) = delete;
  HprofBuffer& operator=(const HprofBuffer&) = delete;

  uint32_t GetOneByte();
  uint32_t GetTwoBytes();
  uint32_t GetFourBytes();
  uint64_t GetId();

  // Read in the next |num_bytes| as an uint32_t.
  uint32_t GetUInt32FromBytes(size_t num_bytes);

  // Read in the next |num_bytes| as an uint64_t.
  uint64_t GetUInt64FromBytes(size_t num_bytes);

  bool HasRemaining();
  void set_id_size(unsigned id_size);
  void set_position(size_t new_position);

  // Skips |delta| bytes in the buffer.
  void Skip(uint32_t delta);

  void SkipBytesByType(DataType type);
  void SkipId();

  // Returns a pointer to the current position of |data_| with offset included.
  const char* DataPosition();
  uint32_t SizeOfType(uint32_t index);
  size_t offset() const { return offset_; }
  unsigned object_id_size_in_bytes() const { return object_id_size_in_bytes_; }

 private:
  unsigned char GetByte();

  // The ID size in bytes of the objects in the hprof, valid values are 4 and 8.
  unsigned object_id_size_in_bytes_ = 4;

  const raw_ptr<const unsigned char, AllowPtrArithmetic>
      data_;                         // Pointer to buffer.
  const size_t size_;                // Total size of buffer.
  size_t offset_ = 0;  // Index into buffer as we parse through contents.
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_BUFFER_ANDROID_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_DATA_TYPE_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_DATA_TYPE_ANDROID_H_

#include <stddef.h>
#include <stdint.h>

namespace tracing {

// These are the possible primitive types that can be read in by the parser.
// They are each associated with an id number that is read in by the parser.
// NOTE: The field values are used to index elements in the arrays below.
enum DataType {
  OBJECT = 2,
  BOOLEAN = 4,
  CHAR = 5,
  FLOAT = 6,
  DOUBLE = 7,
  BYTE = 8,
  SHORT = 9,
  INT = 10,
  LONG = 11
};

// These are the sizes of the above primitive types indexed by their id.
// The OBJECT type has size based off the id size (4 or 8). This is determined
// only after the parser begins running. Thus, it's stored as 0 in this array.
constexpr unsigned kTypeSizes[] = {0, 0, 0, 0, 1, 2, 4, 8, 1, 2, 4, 8};

// These are the string representations of the above primitive types indexed by
// their id.
constexpr const char* kPrimitiveArrayStrings[] = {
    "0",       "0",        "0",      "0",       "bool[]", "char[]",
    "float[]", "double[]", "byte[]", "short[]", "int[]",  "long[]"};

inline const char* GetTypeString(uint32_t index) {
  return kPrimitiveArrayStrings[index];
}

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_HPROF_DATA_TYPE_ANDROID_H_

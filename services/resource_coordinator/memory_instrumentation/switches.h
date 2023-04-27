// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_SWITCHES_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_SWITCHES_H_

namespace memory_instrumentation {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kDisableChromeTracingComputation[];
extern const char kUseHeapProfilingProtoWriter[];

}  // namespace switches
}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_SWITCHES_H_

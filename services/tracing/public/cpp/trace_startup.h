// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/global_descriptors.h"
#endif

namespace base {
class CommandLine;
}  // namespace base

namespace tracing {

inline constexpr uint32_t kStartupTracingTimeoutMs = 30 * 1000;  // 30 sec

// Returns true if `InitTracing()` has been called for this process.
bool COMPONENT_EXPORT(TRACING_CPP) IsTracingInitialized();

// Initializes the perfetto backend and, if startup tracing command line flags
// are present, enables startup tracing with a config based on the flags.
//
// This function may only be called once per process.
//
// This should only be called after sandbox initialization on platforms that
// require single thread. Will switch IsTracingInitialized() to return true.
// `enable_consumer` should be true if the system consumer can be enabled.
// Currently this is only the case if this is running in the browser process.
// `enable_system_backend` enables tracing to the system backend on Posix
// systems, and is ignored on other platforms. The system backend may also be
// enabled if this is a debug Android device.
void COMPONENT_EXPORT(TRACING_CPP)
    InitTracing(bool enable_consumer,
                bool will_trace_thread_restart,
                bool enable_system_backend,
                base::RepeatingCallback<bool()> allow_system_tracing_consumer);

// Calls `InitTracing` with `enable_system_backend` supplied by a feature flag
// check.
void COMPONENT_EXPORT(TRACING_CPP) InitTracingPostFeatureList(
    bool enable_consumer,
    bool will_trace_thread_restart,
    base::RepeatingCallback<bool()> allow_system_tracing_consumer =
        base::NullCallback());

// If tracing is enabled, grabs the current trace config & mode and tells the
// child to begin tracing right away via startup tracing command line flags.

// If tracing is enabled, returns a read-only SMB containing the current tracing
// config, to be forwarded at child processes creation.
base::ReadOnlySharedMemoryRegion COMPONENT_EXPORT(TRACING_CPP)
    CreateTracingConfigSharedMemory();

// If tracing is enabled, returns a writeable SMB as destination of tracing
// data, to be forwarded at child process creation.
base::UnsafeSharedMemoryRegion COMPONENT_EXPORT(TRACING_CPP)
    CreateTracingOutputSharedMemory();

// Tells the child process to begin tracing right away via command line
// flags and launch options, given a SMB config obtained with
// CreateTracingConfigSharedMemory().
void COMPONENT_EXPORT(TRACING_CPP) AddTraceConfigToLaunchParameters(
    const base::ReadOnlySharedMemoryRegion& read_only_memory_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options);

// Tells the child process to write tracing data to this SMB.
void COMPONENT_EXPORT(TRACING_CPP) AddTraceOutputToLaunchParameters(
    const base::UnsafeSharedMemoryRegion& unsafe_memory_region,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

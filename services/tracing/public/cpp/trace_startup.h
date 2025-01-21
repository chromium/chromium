// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"
#include "base/memory/read_only_shared_memory_region.h"
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

// Returns true if InitTracingPostThreadPoolStartAndFeatureList has been called
// for this process.
bool COMPONENT_EXPORT(TRACING_CPP) IsTracingInitialized();

// Hooks up hooks up service callbacks in TraceLog for the perfetto backend and,
// if startup tracing command line flags are present, enables TraceLog with a
// config based on the flags. In zygote children, this should only be called
// after mojo is initialized, as the zygote's sandbox prevents creation of the
// tracing SMB before that point.
//
// TODO(eseckler): Consider allocating the SMB in parent processes outside the
// sandbox and supply it via the command line. Then, we can revert to call this
// earlier and from fewer places again.
void COMPONENT_EXPORT(TRACING_CPP) EnableStartupTracingIfNeeded();

// Enable startup tracing for the current process with the provided config. Sets
// up ProducerClient and trace event and/or sampler profiler data sources, and
// enables TraceLog. The caller should also instruct Chrome's tracing service to
// start tracing, once the service is connected. Returns false on failure.
//
// TODO(eseckler): Figure out what startup tracing APIs should look like with
// the client lib.
bool COMPONENT_EXPORT(TRACING_CPP)
    EnableStartupTracingForProcess(const perfetto::TraceConfig&);

// Initialize tracing components that require task runners. Will switch
// IsTracingInitialized() to return true.
// |enable_consumer| should be true if the system consumer can be enabled.
// Currently this is only the case if this is running in the browser process.
void COMPONENT_EXPORT(TRACING_CPP)
    InitTracingPostThreadPoolStartAndFeatureList(bool enable_consumer);

// If tracing is enabled, grabs the current trace config & mode and tells the
// child to begin tracing right away via startup tracing command line flags.

// If tracing is enabled, returns a read-only SMB containing the current tracing
// config, to be forwarded at child processes creation.
base::ReadOnlySharedMemoryRegion COMPONENT_EXPORT(TRACING_CPP)
    CreateTracingConfigSharedMemory();

// Tells the child process to begin tracing right away via command line
// flags and launch options, given a SMB config obtained with
// CreateTracingConfigSharedMemory().
void COMPONENT_EXPORT(TRACING_CPP) AddTraceConfigToLaunchParameters(
    base::ReadOnlySharedMemoryRegion,
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
    base::GlobalDescriptors::Key descriptor_key,
    base::ScopedFD& out_descriptor_to_share,
#endif
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

#include "base/component_export.h"

namespace base {
class CommandLine;

namespace trace_event {
class TraceConfig;
}  // namespace trace_event
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
    EnableStartupTracingForProcess(const base::trace_event::TraceConfig&,
                                   bool privacy_filtering_enabled);

// Initialize tracing components that require task runners. Will switch
// IsTracingInitialized() to return true.
void COMPONENT_EXPORT(TRACING_CPP)
    InitTracingPostThreadPoolStartAndFeatureList();

// If tracing is enabled, grabs the current trace config & mode and tells the
// child to begin tracing right away via startup tracing command line flags.
void COMPONENT_EXPORT(TRACING_CPP)
    PropagateTracingFlagsToChildProcessCmdLine(base::CommandLine* cmd_line);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_STARTUP_H_

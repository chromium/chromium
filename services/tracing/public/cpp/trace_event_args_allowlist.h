// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_ARGS_ALLOWLIST_H_
#define SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_ARGS_ALLOWLIST_H_

#include <string>

#include "base/component_export.h"
#include "base/trace_event/trace_event_impl.h"

namespace tracing {

// TODO(ssid): This is a temporary argument filter that will be removed once
// slow reports moves to using proto completely.

// Used to filter trace event arguments against a allowlist of events that
// have been manually vetted to not include any PII.
bool COMPONENT_EXPORT(TRACING_CPP) IsTraceEventArgsAllowlisted(
    const char* category_group_name,
    const char* event_name,
    base::trace_event::ArgumentNameFilterPredicate* arg_name_filter);

// Used to filter metadata against a allowlist of metadata names that have been
// manually vetted to not include any PII.
bool COMPONENT_EXPORT(TRACING_CPP)
    IsMetadataAllowlisted(const std::string& metadata_name);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_TRACE_EVENT_ARGS_ALLOWLIST_H_

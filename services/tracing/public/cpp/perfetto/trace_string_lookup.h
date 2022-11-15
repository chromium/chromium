// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_

#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_thread_descriptor.pbzero.h"

namespace tracing {

perfetto::protos::pbzero::ChromeThreadDescriptor::ThreadType GetThreadType(
    const char* const thread_name);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_

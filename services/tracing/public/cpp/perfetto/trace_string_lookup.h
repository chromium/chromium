// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_

#include "base/tracing/protos/chrome_enums.pbzero.h"

namespace tracing {

perfetto::protos::chrome_enums::pbzero::ThreadType GetThreadType(
    const char* const thread_name);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_STRING_LOOKUP_H_

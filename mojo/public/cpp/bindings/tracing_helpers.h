// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TRACING_HELPERS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TRACING_HELPERS_H_

#include "base/trace_event/trace_event.h"
#include "build/buildflag.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"

// Helper for determine trace category for high-level coarse mojo events:
// - If detailed mojo tracing is enabled, these events are not useful in most
// circumstances (e.g. they capture only interface name, while detailed mojo
// event tracing emits events which contain both interface and method names)
// apart from debugging mojo or trace events, so "disabled-by-default-mojom"
// category is used.
// - If detailed mojo tracing is disabled, then the passed category is used.
#if BUILDFLAG(MOJO_TRACE_ENABLED)
#define TRACE_CATEGORY_OR_DISABLED_BY_DEFAULT_MOJOM(category) \
  TRACE_DISABLED_BY_DEFAULT("mojom")
#else
#define TRACE_CATEGORY_OR_DISABLED_BY_DEFAULT_MOJOM(category) category
#endif

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TRACING_HELPERS_H_

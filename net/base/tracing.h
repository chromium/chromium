// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRACING_H_
#define NET_BASE_TRACING_H_

#include "build/build_config.h"
#include "net/base/cronet_buildflags.h"

#if BUILDFLAG(CRONET_BUILD) && !BUILDFLAG(IS_APPLE)
#include "net/base/trace_event_stub.h"  // IWYU pragma: export
#endif  // BUILDFLAG(CRONET_BUILD) && !BUILDFLAG(IS_APPLE)

#include "base/trace_event/base_tracing.h"  // IWYU pragma: export

#endif  // NET_BASE_TRACING_H_

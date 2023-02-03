// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/traced_process.h"

#include <utility>

#include "build/blink_buildflags.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_NACL) && BUILDFLAG(USE_BLINK)
#include "services/tracing/public/cpp/traced_process_impl.h"
#endif

namespace tracing {

// static
void TracedProcess::ResetTracedProcessReceiver() {
#if !BUILDFLAG(IS_NACL) && BUILDFLAG(USE_BLINK)
  tracing::TracedProcessImpl::GetInstance()->ResetTracedProcessReceiver();
#endif
}

// static
void TracedProcess::OnTracedProcessRequest(
    mojo::PendingReceiver<mojom::TracedProcess> receiver) {
#if !BUILDFLAG(IS_NACL) && BUILDFLAG(USE_BLINK)
  tracing::TracedProcessImpl::GetInstance()->OnTracedProcessRequest(
      std::move(receiver));
#endif
}

// static
void TracedProcess::EnableSystemTracingService(
    mojo::PendingRemote<mojom::SystemTracingService> remote) {
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_NACL) && \
    BUILDFLAG(USE_BLINK)
  tracing::TracedProcessImpl::GetInstance()->EnableSystemTracingService(
      std::move(remote));
#endif
}

}  // namespace tracing

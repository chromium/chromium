// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/traced_process.h"

#include <utility>

#include "build/build_config.h"

#if !defined(OS_NACL) && !defined(OS_IOS)
#include "services/tracing/public/cpp/traced_process_impl.h"
#endif

namespace tracing {

// static
void TracedProcess::ResetTracedProcessReceiver() {
#if !defined(OS_NACL) && !defined(OS_IOS)
  tracing::TracedProcessImpl::GetInstance()->ResetTracedProcessReceiver();
#endif
}

// static
void TracedProcess::OnTracedProcessRequest(
    mojo::PendingReceiver<mojom::TracedProcess> receiver) {
#if !defined(OS_NACL) && !defined(OS_IOS)
  tracing::TracedProcessImpl::GetInstance()->OnTracedProcessRequest(
      std::move(receiver));
#endif
}

}  // namespace tracing

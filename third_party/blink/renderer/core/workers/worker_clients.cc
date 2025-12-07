// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_clients.h"

namespace blink {

void WorkerClients::Trace(Visitor* visitor) const {
  visitor->Trace(animation_worklet_proxy_client_);
  visitor->Trace(paint_worklet_proxy_client_);
}

}  // namespace blink

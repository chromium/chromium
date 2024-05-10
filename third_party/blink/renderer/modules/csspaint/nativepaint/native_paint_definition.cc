// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/nativepaint/native_paint_definition.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

NativePaintDefinition::NativePaintDefinition(
    LocalFrame* local_root,
    PaintWorkletInput::PaintWorkletInputType type)
    : worklet_id_(PaintWorkletIdGenerator::NextId()) {
  DCHECK(local_root->IsLocalRoot());
  DCHECK(IsMainThread());
  RegisterProxyClient(local_root, type);
}

void NativePaintDefinition::RegisterProxyClient(
    LocalFrame* local_root,
    PaintWorkletInput::PaintWorkletInputType type) {
  proxy_client_ =
      PaintWorkletProxyClient::Create(local_root->DomWindow(), worklet_id_);
  proxy_client_->RegisterForNativePaintWorklet(/*thread=*/nullptr, this, type);
}

void NativePaintDefinition::UnregisterProxyClient() {
  proxy_client_->UnregisterForNativePaintWorklet();
}

void NativePaintDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(proxy_client_);
  PaintDefinition::Trace(visitor);
}

int NativePaintDefinition::GetWorkletId() const {
  return worklet_id_;
}

}  // namespace blink

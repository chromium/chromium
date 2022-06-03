// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/peerconnection/execution_context_metronome_provider.h"

#include "base/memory/scoped_refptr.h"

namespace blink {

// static
const char ExecutionContextMetronomeProvider::kSupplementName[] =
    "ExecutionContextMetronomeProvider";

// static
ExecutionContextMetronomeProvider& ExecutionContextMetronomeProvider::From(
    ExecutionContext& context) {
  CHECK(!context.IsContextDestroyed());
  ExecutionContextMetronomeProvider* supplement =
      Supplement<ExecutionContext>::From<ExecutionContextMetronomeProvider>(
          context);
  if (!supplement) {
    supplement =
        MakeGarbageCollected<ExecutionContextMetronomeProvider>(context);
    ProvideTo(context, supplement);
  }
  return *supplement;
}

ExecutionContextMetronomeProvider::ExecutionContextMetronomeProvider(
    ExecutionContext& context)
    : Supplement(context),
      metronome_provider_(base::MakeRefCounted<MetronomeProvider>()) {}

scoped_refptr<MetronomeProvider>
ExecutionContextMetronomeProvider::metronome_provider() const {
  return metronome_provider_;
}

void ExecutionContextMetronomeProvider::Trace(Visitor* visitor) const {
  Supplement<ExecutionContext>::Trace(visitor);
}

}  // namespace blink

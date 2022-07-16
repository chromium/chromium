// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PEERCONNECTION_EXECUTION_CONTEXT_METRONOME_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PEERCONNECTION_EXECUTION_CONTEXT_METRONOME_PROVIDER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/webrtc_overrides/metronome_provider.h"

namespace blink {

// Supplements the ExecutionContext with a MetronomeProvider. In practise, the
// provider is controlled by the PeerConnectionDependencyFactory.
class CORE_EXPORT ExecutionContextMetronomeProvider final
    : public GarbageCollected<ExecutionContextMetronomeProvider>,
      public Supplement<ExecutionContext> {
 public:
  static const char kSupplementName[];

  // Gets or creates the ExecutionContextMetronomeProvider.
  static ExecutionContextMetronomeProvider& From(ExecutionContext&);
  explicit ExecutionContextMetronomeProvider(ExecutionContext&);

  // Gets the context's metronome provider.
  scoped_refptr<MetronomeProvider> metronome_provider() const;

  void Trace(Visitor*) const override;

 private:
  const scoped_refptr<MetronomeProvider> metronome_provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PEERCONNECTION_EXECUTION_CONTEXT_METRONOME_PROVIDER_H_

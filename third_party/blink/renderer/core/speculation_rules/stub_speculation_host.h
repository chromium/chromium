// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_

namespace blink {

class StubSpeculationHost : public mojom::blink::SpeculationHost {
 public:
  using Candidates = Vector<mojom::blink::SpeculationCandidatePtr>;

  const Candidates& candidates() const { return candidates_; }
  void SetDoneClosure(base::OnceClosure done) {
    done_closure_ = std::move(done);
  }

  void BindUnsafe(mojo::ScopedMessagePipeHandle handle);
  void Bind(mojo::PendingReceiver<SpeculationHost> receiver);

  // mojom::blink::SpeculationHost.
  void UpdateSpeculationCandidates(Candidates candidates) override;

  void OnConnectionLost();

  bool is_bound() const { return receiver_.is_bound(); }

 private:
  mojo::Receiver<SpeculationHost> receiver_{this};
  Vector<mojom::blink::SpeculationCandidatePtr> candidates_;
  base::OnceClosure done_closure_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_STUB_SPECULATION_HOST_H_

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/stub_speculation_host.h"

#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

void StubSpeculationHost::BindUnsafe(mojo::ScopedMessagePipeHandle handle) {
  Bind(mojo::PendingReceiver<SpeculationHost>(std::move(handle)));
}

void StubSpeculationHost::Bind(
    mojo::PendingReceiver<SpeculationHost> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &StubSpeculationHost::OnConnectionLost, WTF::Unretained(this)));
}

void StubSpeculationHost::OnConnectionLost() {
  if (done_closure_)
    std::move(done_closure_).Run();
}

void StubSpeculationHost::UpdateSpeculationCandidates(Candidates candidates) {
  candidates_ = std::move(candidates);
  if (candidates_updated_callback_) {
    candidates_updated_callback_.Run(candidates_);
  }
  if (done_closure_)
    std::move(done_closure_).Run();
}

void StubSpeculationHost::InitiatePreview(const KURL& url) {}

}  // namespace blink

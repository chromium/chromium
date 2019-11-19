// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_local_frame.h"

namespace content {

FakeLocalFrame::FakeLocalFrame() {}

FakeLocalFrame::~FakeLocalFrame() {}

void FakeLocalFrame::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FakeLocalFrame::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrame::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  std::move(callback).Run(base::string16(), 0, 0);
}

void FakeLocalFrame::SendInterventionReport(const std::string& id,
                                            const std::string& message) {}

void FakeLocalFrame::NotifyUserActivation() {}

void FakeLocalFrame::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {}

void FakeLocalFrame::CheckCompleted() {}

void FakeLocalFrame::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
      std::move(handle)));
}

}  // namespace content

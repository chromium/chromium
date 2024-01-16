// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"

namespace on_device_model {

TestResponseHolder::TestResponseHolder() = default;

TestResponseHolder::~TestResponseHolder() = default;

mojo::PendingRemote<mojom::StreamingResponder>
TestResponseHolder::BindRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void TestResponseHolder::WaitForCompletion() {
  run_loop_.Run();
}

void TestResponseHolder::OnResponse(mojom::ResponseChunkPtr chunk) {
  responses_.push_back(chunk->text);
}

void TestResponseHolder::OnComplete(mojom::ResponseSummaryPtr summary) {
  run_loop_.Quit();
}

}  // namespace on_device_model

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"

#include "base/functional/bind.h"

namespace on_device_model {

TestResponseHolder::TestResponseHolder() = default;

TestResponseHolder::~TestResponseHolder() = default;

mojo::PendingRemote<mojom::StreamingResponder>
TestResponseHolder::BindRemote() {
  auto remote = receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(base::BindOnce(
      &TestResponseHolder::OnDisconnect, base::Unretained(this)));
  complete_ = false;
  disconnected_ = false;
  return remote;
}

void TestResponseHolder::WaitForCompletion() {
  run_loop_.Run();
}

void TestResponseHolder::OnResponse(mojom::ResponseChunkPtr chunk) {
  responses_.push_back(chunk->text);
}

void TestResponseHolder::OnComplete(mojom::ResponseSummaryPtr summary) {
  complete_ = true;
  input_token_count_ = summary->input_token_count;
  output_token_count_ = summary->output_token_count;
  run_loop_.Quit();
}

void TestResponseHolder::OnDisconnect() {
  disconnected_ = true;
  run_loop_.Quit();
}

}  // namespace on_device_model

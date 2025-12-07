// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/test_support/test_response_holder.h"

#include "base/functional/bind.h"

namespace on_device_model {

TestResponseHolder::TestResponseHolder() = default;

TestResponseHolder::~TestResponseHolder() = default;

void TestResponseHolder::OnResponse(mojom::ResponseChunkPtr chunk) {
  responses_.push_back(chunk->text);
}

void TestResponseHolder::OnComplete(mojom::ResponseSummaryPtr summary) {
  complete_ = true;
  output_token_count_ = summary->output_token_count;
  OnCompleted();
}

TestAsrResponseHolder::TestAsrResponseHolder() = default;
TestAsrResponseHolder::~TestAsrResponseHolder() = default;

void TestAsrResponseHolder::OnResponse(
    std::vector<mojom::SpeechRecognitionResultPtr> results) {
  for (const auto& r : results) {
    responses_.push_back(r->transcript);
  }
  if (wait_for_response_) {
    wait_for_response_ = false;
    OnCompleted();
  }
}

}  // namespace on_device_model

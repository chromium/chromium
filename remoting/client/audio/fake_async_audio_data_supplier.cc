// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/audio/fake_async_audio_data_supplier.h"

#include <string.h>
#include <utility>

#include "base/check_op.h"

namespace remoting {

FakeAsyncAudioDataSupplier::FakeAsyncAudioDataSupplier() = default;

FakeAsyncAudioDataSupplier::~FakeAsyncAudioDataSupplier() = default;

void FakeAsyncAudioDataSupplier::AsyncGetData(
    std::unique_ptr<GetDataRequest> request) {
  pending_requests_.push_back(std::move(request));

  if (fulfill_requests_immediately_) {
    FulfillAllRequests();
  }
}

void FakeAsyncAudioDataSupplier::ClearGetDataRequests() {
  pending_requests_.clear();
}

void FakeAsyncAudioDataSupplier::FulfillNextRequest() {
  DCHECK_GT(pending_requests_count(), 0u);
  auto& request = pending_requests_.front();
  memset(request->data, kDummyAudioData, request->bytes_needed);
  request->OnDataFilled();
  pending_requests_.pop_front();
  fulfilled_requests_count_++;
}

void FakeAsyncAudioDataSupplier::FulfillAllRequests() {
  while (pending_requests_count() > 0) {
    FulfillNextRequest();
  }
}

void FakeAsyncAudioDataSupplier::ResetFulfilledRequestsCounter() {
  fulfilled_requests_count_ = 0u;
}

}  // namespace remoting

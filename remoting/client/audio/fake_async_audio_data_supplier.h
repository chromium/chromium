// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_FAKE_ASYNC_AUDIO_DATA_SUPPLIER_H_
#define REMOTING_CLIENT_AUDIO_FAKE_ASYNC_AUDIO_DATA_SUPPLIER_H_

#include <cstdint>
#include <list>
#include <memory>

#include "remoting/client/audio/async_audio_data_supplier.h"

namespace remoting {

// A fake AsyncAudioDataSupplier implementation for testing.
class FakeAsyncAudioDataSupplier : public AsyncAudioDataSupplier {
 public:
  // Dummy audio data that will be filled into pending requests.
  const uint8_t kDummyAudioData = 0x8b;

  FakeAsyncAudioDataSupplier();

  FakeAsyncAudioDataSupplier(const FakeAsyncAudioDataSupplier&) = delete;
  FakeAsyncAudioDataSupplier& operator=(const FakeAsyncAudioDataSupplier&) =
      delete;

  ~FakeAsyncAudioDataSupplier() override;

  // AsyncAudioDataSupplier implementations.
  void AsyncGetData(std::unique_ptr<GetDataRequest> request) override;
  void ClearGetDataRequests() override;

  // Fulfills the next pending request by filling it with |kDummyAudioData|.
  void FulfillNextRequest();

  // Fulfills all pending requests.
  void FulfillAllRequests();

  // Resets fulfilled_requests_count() to 0.
  void ResetFulfilledRequestsCounter();

  // Returns number of requests that are not fulfilled.
  size_t pending_requests_count() const { return pending_requests_.size(); }

  // Returns number of requests that have been fulfilled. Can be reset to 0 by
  // calling ResetFulfilledRequestsCounter().
  size_t fulfilled_requests_count() const { return fulfilled_requests_count_; }

  // If this is true, the instance will immediately fulfill get-data requests
  // when AsyncGetData is called, otherwise the caller needs to call
  // FulfillNextRequest() or FulfillAllRequests() to fulfill the requests.
  // The default value is false.
  void set_fulfill_requests_immediately(bool fulfill_requests_immediately) {
    fulfill_requests_immediately_ = fulfill_requests_immediately;
  }

 private:
  bool fulfill_requests_immediately_ = false;
  std::list<std::unique_ptr<GetDataRequest>> pending_requests_;
  size_t fulfilled_requests_count_ = 0u;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_FAKE_ASYNC_AUDIO_DATA_SUPPLIER_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_AUDIO_ASYNC_AUDIO_DATA_SUPPLIER_H_
#define REMOTING_CLIENT_AUDIO_ASYNC_AUDIO_DATA_SUPPLIER_H_

#include <memory>

namespace remoting {

// This interface allows caller to asynchronously request for audio data.
class AsyncAudioDataSupplier {
 public:
  class GetDataRequest {
   public:
    // |data| must outlive |this|.
    GetDataRequest(void* data, size_t bytes_needed);
    virtual ~GetDataRequest();

    // Called when |data| has been filled with |bytes_needed| bytes of data.
    //
    // Caution: Do not add or drop requests (i.e. calling AsyncGetData() or
    // ClearGetDataRequests()) directly inside OnDataFilled(), which has
    // undefined behavior. Consider posting a task when necessary.
    virtual void OnDataFilled() = 0;

    void* const data;
    const size_t bytes_needed;

    size_t bytes_extracted = 0;
  };

  AsyncAudioDataSupplier();
  virtual ~AsyncAudioDataSupplier();

  // Requests for more data from the supplier.
  virtual void AsyncGetData(std::unique_ptr<GetDataRequest> request) = 0;

  // Drops all pending get-data requests. You may want to call this before the
  // caller is destroyed if the caller has a shorter lifetime than the supplier.
  virtual void ClearGetDataRequests() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_AUDIO_ASYNC_AUDIO_DATA_SUPPLIER_H_

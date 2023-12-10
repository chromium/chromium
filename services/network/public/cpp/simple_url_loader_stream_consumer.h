// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_STREAM_CONSUMER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_STREAM_CONSUMER_H_

#include <string_view>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace network {

// Interface to handle streaming data from SimpleURLLoader. All methods are
// invoked on the sequence the SimpleURLLoader was started on, and all callbacks
// must be invoked on the same sequence. The SimpleURLLoader may be deleted at
// any time, including in any of the callbacks it invokes. None of these methods
// will be called during SimpleURLLoader destruction.
class COMPONENT_EXPORT(NETWORK_CPP) SimpleURLLoaderStreamConsumer {
 public:
  SimpleURLLoaderStreamConsumer(const SimpleURLLoaderStreamConsumer&) = delete;
  SimpleURLLoaderStreamConsumer& operator=(
      const SimpleURLLoaderStreamConsumer&) = delete;

  // Called as body data is received.
  //
  // More data will not be read until |resume| is called. It's safe to call
  // |resume| synchronously, and to delete the SimpleURLLoader during the call.
  //
  // |string_piece| will only be valid for the duration of the OnDataReceived()
  // call, but does remain valid during that call even if the SimpleURLLoader
  // is destroyed. |string_piece| will never be of length 0.
  //
  // In the case of error, all data received over the pipe will be passed to
  // this method before calling OnComplete, even if partial responses are set to
  // be treated as errors (the default behavior), as it may not yet be known if
  // the request will succeed or fail.
  virtual void OnDataReceived(std::string_view string_piece,
                              base::OnceClosure resume) = 0;

  // Called on successful completion, or error. In the default configuration,
  // |success| is true if the request received a 2xx response and the entire
  // response body, false otherwise. Allowing partial responses or HTTP errors
  // will affect the value of |success|.
  //
  // 4xx and 5xx responses are considered successes, if
  // SimpleURLLoader::SetAllowHttpErrorResults() is set to true.
  //
  // When the SimpleURLLoader retries a request, OnComplete() is only called
  // after the final retry.
  virtual void OnComplete(bool success) = 0;

  // When retries are enabled for a request, this is called before the
  // SimpleURLLoader retries. If this is called after data has been received,
  // the entire response of the retried request will be passed to
  // OnDataReceived() again, and any previously received partial response should
  // be discarded.
  //
  // The request will not be retried until start_retry is invoked, which may be
  // done synchronously or asynchronously. It's safe to delete the
  // SimpleURLLoader during this call.
  virtual void OnRetry(base::OnceClosure start_retry) = 0;

 protected:
  SimpleURLLoaderStreamConsumer() {}
  virtual ~SimpleURLLoaderStreamConsumer() {}
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SIMPLE_URL_LOADER_STREAM_CONSUMER_H_

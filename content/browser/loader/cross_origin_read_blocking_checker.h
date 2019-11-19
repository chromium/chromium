// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_
#define CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_errors.h"
#include "services/network/cross_origin_read_blocking.h"

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace network {
struct ResourceRequest;
struct ResourceResponseHead;
}  // namespace network

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace url {
class Origin;
}  // namespace url

namespace content {

// This class checks whether we should block the response or not using
// CrossOriginReadBlocking::ResponseAnalyzer.
class CrossOriginReadBlockingChecker {
 public:
  enum class Result {
    kAllowed,
    kBlocked_ShouldReport,
    kBlocked_ShouldNotReport,
    kNetError
  };
  CrossOriginReadBlockingChecker(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& response,
      const url::Origin& request_initiator_site_lock,
      const storage::BlobDataHandle& blob_data_handle,
      base::OnceCallback<void(Result)> callback);
  ~CrossOriginReadBlockingChecker();

  int GetNetError();

 private:
  class BlobIOState;

  void OnAllowed();
  void OnBlocked();
  void OnNetError(int net_error);

  void OnReadComplete(int bytes_read,
                      scoped_refptr<net::IOBufferWithSize> buffer,
                      int net_error);

  base::OnceCallback<void(Result)> callback_;
  std::unique_ptr<network::CrossOriginReadBlocking::ResponseAnalyzer>
      corb_analyzer_;
  std::unique_ptr<BlobIOState> blob_io_state_;
  int net_error_ = net::OK;

  base::WeakPtrFactory<CrossOriginReadBlockingChecker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingChecker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_

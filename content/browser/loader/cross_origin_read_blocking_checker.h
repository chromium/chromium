// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_
#define CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/orb/orb_api.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
class IOBufferWithSize;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

namespace storage {
class BlobDataHandle;
}  // namespace storage

namespace content {

// This class checks whether we should block the response or not using ORB
// (Opaque Response Blocking).
class CrossOriginReadBlockingChecker {
 public:
  enum class Result {
    kAllowed,
    kBlocked_ShouldReport,
    kBlocked_ShouldNotReport,
    kNetError
  };
  // The caller needs to guarantee that `orb_state`'s lifetime is at least as
  // long as the lifetime of `CrossOriginReadBlockingChecker`.  `orb_state`
  // needs to be non-null.
  CrossOriginReadBlockingChecker(
      const network::ResourceRequest& request,
      const network::mojom::URLResponseHead& response,
      const storage::BlobDataHandle& blob_data_handle,
      network::orb::PerFactoryState* orb_state,
      base::OnceCallback<void(Result)> callback);

  CrossOriginReadBlockingChecker(const CrossOriginReadBlockingChecker&) =
      delete;
  CrossOriginReadBlockingChecker& operator=(
      const CrossOriginReadBlockingChecker&) = delete;

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
  std::unique_ptr<network::orb::ResponseAnalyzer> orb_analyzer_;
  std::unique_ptr<BlobIOState> blob_io_state_;
  int net_error_ = net::OK;

  base::WeakPtrFactory<CrossOriginReadBlockingChecker> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CROSS_ORIGIN_READ_BLOCKING_CHECKER_H_

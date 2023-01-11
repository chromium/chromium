// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_BASE_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_BASE_H_

#include <memory>
#include <string>

#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/scoped_protobuf_http_request.h"

namespace network {

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom

class SimpleURLLoader;

}  // namespace network

namespace remoting {

class ProtobufHttpClient;

struct ProtobufHttpRequestConfig;

// Base request class for unary and server streaming requests.
class ProtobufHttpRequestBase {
 public:
  explicit ProtobufHttpRequestBase(
      std::unique_ptr<ProtobufHttpRequestConfig> config);
  virtual ~ProtobufHttpRequestBase();

  // Creates an ScopedProtobufHttpRequest instance that will cancel the
  // request once the instance is deleted. It will be no-op if the request is
  // not in the client's pending request list.
  std::unique_ptr<ScopedProtobufHttpRequest> CreateScopedRequest();

  const ProtobufHttpRequestConfig& config() const { return *config_; }

 protected:
  // Called when the protobuf HTTP client failed to get the access token. Note
  // that the subclass implementation should not invoke |invalidator_| since the
  // request has never been started.
  virtual void OnAuthFailed(const ProtobufHttpStatus& status) = 0;

  virtual void StartRequestInternal(
      network::mojom::URLLoaderFactory* loader_factory) = 0;

  // Returns a deadline for when the request has to be finished. Returns zero
  // if the request doesn't timeout. This is generally only useful for unary
  // requests.
  virtual base::TimeDelta GetRequestTimeoutDuration() const = 0;

  // Returns the http status from |url_loader_|. Only useful when |url_loader_|
  // informs that the request has been completed.
  ProtobufHttpStatus GetUrlLoaderStatus() const;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Subclass should run this closure whenever its lifetime ends, e.g. response
  // is received or stream is closed. This will delete |this| from the parent
  // ProtobufHttpClient.
  base::OnceClosure invalidator_;

 private:
  friend class ProtobufHttpClient;

  // Called by ProtobufHttpClient.
  void StartRequest(network::mojom::URLLoaderFactory* loader_factory,
                    std::unique_ptr<network::SimpleURLLoader> url_loader,
                    base::OnceClosure invalidator);

  void Invalidate();

  std::unique_ptr<ProtobufHttpRequestConfig> config_;

#if DCHECK_IS_ON()
  base::TimeTicks request_deadline_;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<ProtobufHttpRequestBase> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_BASE_H_

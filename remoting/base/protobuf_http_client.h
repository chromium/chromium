// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_
#define REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_

#include <list>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/url_loader_network_service_observer.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

class ProtobufHttpRequestBase;

// Helper class for executing REST/Protobuf requests over HTTP.
class ProtobufHttpClient final {
 public:
  // |server_endpoint| is the hostname of the server.
  // |token_getter| is nullable if none of the requests are authenticated.
  // |token_getter| must outlive |this|.
  ProtobufHttpClient(
      const std::string& server_endpoint,
      OAuthTokenGetter* token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ProtobufHttpClient();
  ProtobufHttpClient(const ProtobufHttpClient&) = delete;
  ProtobufHttpClient& operator=(const ProtobufHttpClient&) = delete;

  // Executes a unary request. Caller will not be notified of the result if
  // CancelPendingRequests() is called or |this| is destroyed.
  void ExecuteRequest(std::unique_ptr<ProtobufHttpRequestBase> request);

  // Cancel all pending requests.
  void CancelPendingRequests();

  // Indicates whether the client has any pending requests.
  bool HasPendingRequests() const;

 private:
  using PendingRequestList =
      std::list<std::unique_ptr<ProtobufHttpRequestBase>>;

  // std::list iterators are stable, so they survive list editing and only
  // become invalidated when underlying element is deleted.
  using PendingRequestListIterator = PendingRequestList::iterator;

  void DoExecuteRequest(std::unique_ptr<ProtobufHttpRequestBase> request,
                        OAuthTokenGetter::Status status,
                        const std::string& user_email,
                        const std::string& access_token,
                        const std::string& scopes);

  void CancelRequest(const PendingRequestListIterator& request_iterator);

  std::string server_endpoint_;
  raw_ptr<OAuthTokenGetter> token_getter_;
  std::optional<UrlLoaderNetworkServiceObserver> service_observer_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  PendingRequestList pending_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProtobufHttpClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_CLIENT_H_

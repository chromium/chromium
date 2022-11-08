// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that stores the common state between HttpBasicStream and
// WebSocketBasicHandshakeStream.

#ifndef NET_HTTP_HTTP_BASIC_STATE_H_
#define NET_HTTP_HTTP_BASIC_STATE_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace net {

class ClientSocketHandle;
class GrowableIOBuffer;
class HttpStreamParser;
struct HttpRequestInfo;
class NetLogWithSource;

class NET_EXPORT_PRIVATE HttpBasicState {
 public:
  HttpBasicState(std::unique_ptr<ClientSocketHandle> connection,
                 bool using_proxy);

  HttpBasicState(const HttpBasicState&) = delete;
  HttpBasicState& operator=(const HttpBasicState&) = delete;

  ~HttpBasicState();

  // Initialize() must be called before using any of the other methods.
  void Initialize(const HttpRequestInfo* request_info,
                  RequestPriority priority,
                  const NetLogWithSource& net_log);

  HttpStreamParser* parser() const { return parser_.get(); }

  bool using_proxy() const { return using_proxy_; }

  // Deletes |parser_| and sets it to NULL.
  void DeleteParser();

  ClientSocketHandle* connection() const { return connection_.get(); }

  std::unique_ptr<ClientSocketHandle> ReleaseConnection();

  scoped_refptr<GrowableIOBuffer> read_buf() const;

  // Generates a string of the form "METHOD PATH HTTP/1.1\r\n", based on the
  // values of request_info_ and using_proxy_.
  std::string GenerateRequestLine() const;

  MutableNetworkTrafficAnnotationTag traffic_annotation() {
    return traffic_annotation_;
  }

  // Returns true if the connection has been "reused" as defined by HttpStream -
  // either actually reused, or has not been used yet, but has been idle for
  // some time.
  //
  // TODO(mmenke): Consider renaming this concept, to avoid confusion with
  // ClientSocketHandle::is_reused().
  bool IsConnectionReused() const;

  // Retrieves any DNS aliases for the remote endpoint. Includes all known
  // aliases, e.g. from A, AAAA, or HTTPS, not just from the address used for
  // the connection, in no particular order.
  const std::set<std::string>& GetDnsAliases() const;

 private:
  scoped_refptr<GrowableIOBuffer> read_buf_;

  std::unique_ptr<ClientSocketHandle> connection_;

  std::unique_ptr<HttpStreamParser> parser_;

  const bool using_proxy_;

  GURL url_;
  std::string request_method_;

  MutableNetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_BASIC_STATE_H_

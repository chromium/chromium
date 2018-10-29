// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpStream provides an abstraction for a basic http streams, SPDY, and QUIC.
// The HttpStream subtype is expected to manage the underlying transport
// appropriately.  For example, a basic http stream will return the transport
// socket to the pool for reuse.  SPDY streams on the other hand leave the
// transport socket management to the SpdySession.

#ifndef NET_HTTP_HTTP_STREAM_H_
#define NET_HTTP_HTTP_STREAM_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_error_details.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/http/http_raw_request_headers.h"

namespace net {

struct AlternativeService;
class HttpNetworkSession;
class HttpRequestHeaders;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class IPEndPoint;
struct LoadTimingInfo;
class NetLogWithSource;
class SSLCertRequestInfo;
class SSLInfo;

class NET_EXPORT_PRIVATE HttpStream {
 public:
  HttpStream() {}
  virtual ~HttpStream() {}

  // Initialize stream.  Must be called before calling SendRequest().
  // The consumer should ensure that request_info points to a valid value till
  // final response headers are received; after that point, the HttpStream
  // will not access |*request_info| and it may be deleted. If |can_send_early|
  // is true, this stream may send data early without confirming the handshake
  // if this is a resumption of a previously established connection.
  // Returns a net error code, possibly ERR_IO_PENDING.
  virtual int InitializeStream(const HttpRequestInfo* request_info,
                               bool can_send_early,
                               RequestPriority priority,
                               const NetLogWithSource& net_log,
                               CompletionOnceCallback callback) = 0;

  // Writes the headers and uploads body data to the underlying socket.
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback
  // when available. Returns OK on success.
  //
  // Some fields in |response| may be filled by this method, but it will not
  // contain complete information until ReadResponseHeaders returns.
  //
  // |response| must remain valid until all sets of headers has been read, or
  // the HttpStream is destroyed. There's typically only one set of
  // headers, except in the case of 1xx responses (See ReadResponseHeaders).
  virtual int SendRequest(const HttpRequestHeaders& request_headers,
                          HttpResponseInfo* response,
                          CompletionOnceCallback callback) = 0;

  // Reads from the underlying socket until the next set of response headers
  // have been completely received. Normally this is called once per request,
  // however it may be called again in the event of a 1xx response to read the
  // next set of headers.
  //
  // ERR_IO_PENDING is returned if the operation could not be completed
  // synchronously, in which case the result will be passed to the callback when
  // available. Returns OK on success. The response headers are available in
  // the HttpResponseInfo passed in the original call to SendRequest.
  virtual int ReadResponseHeaders(CompletionOnceCallback callback) = 0;

  // Reads response body data, up to |buf_len| bytes. |buf_len| should be a
  // reasonable size (<2MB). The number of bytes read is returned, or an
  // error is returned upon failure.  0 indicates that the request has been
  // fully satisfied and there is no more data to read.
  // ERR_CONNECTION_CLOSED is returned when the connection has been closed
  // prematurely.  ERR_IO_PENDING is returned if the operation could not be
  // completed synchronously, in which case the result will be passed to the
  // callback when available. If the operation is not completed immediately,
  // the socket acquires a reference to the provided buffer until the callback
  // is invoked or the socket is destroyed.
  virtual int ReadResponseBody(IOBuffer* buf,
                               int buf_len,
                               CompletionOnceCallback callback) = 0;

  // Closes the stream.
  // |not_reusable| indicates if the stream can be used for further requests.
  // In the case of HTTP, where we re-use the byte-stream (e.g. the connection)
  // this means we need to close the connection; in the case of SPDY, where the
  // underlying stream is never reused, it has no effect.
  // TODO(mmenke): We should fold the |not_reusable| flag into the stream
  //               implementation itself so that the caller does not need to
  //               pass it at all.  Ideally we'd be able to remove
  //               CanReuseConnection() and IsResponseBodyComplete().
  // TODO(mmenke): We should try and merge Drain() into this method as well.
  virtual void Close(bool not_reusable) = 0;

  // Indicates if the response body has been completely read.
  virtual bool IsResponseBodyComplete() const = 0;

  // A stream exists on top of a connection.  If the connection has been used
  // to successfully exchange data in the past, error handling for the
  // stream is done differently.  This method returns true if the underlying
  // connection is reused or has been connected and idle for some time.
  virtual bool IsConnectionReused() const = 0;
  // TODO(mmenke): We should fold this into RenewStreamForAuth(), and make that
  //    method drain the stream as well, if needed (And return asynchronously).
  virtual void SetConnectionReused() = 0;

  // Checks whether the underlying connection can be reused.  The stream's
  // connection can be reused if the response headers allow for it, the socket
  // is still connected, and the stream exclusively owns the underlying
  // connection.  SPDY and QUIC streams don't own their own connections, so
  // always return false.
  virtual bool CanReuseConnection() const = 0;

  // Get the total number of bytes received from network for this stream.
  virtual int64_t GetTotalReceivedBytes() const = 0;

  // Get the total number of bytes sent over the network for this stream.
  virtual int64_t GetTotalSentBytes() const = 0;

  // Populates the connection establishment part of |load_timing_info|, and
  // socket ID.  |load_timing_info| must have all null times when called.
  // Returns false and does nothing if there is no underlying connection, either
  // because one has yet to be assigned to the stream, or because the underlying
  // socket has been closed.
  //
  // In practice, this means that this function will always succeed any time
  // between when the full headers have been received and the stream has been
  // closed.
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const = 0;

  // Get the SSLInfo associated with this stream's connection.  This should
  // only be called for streams over SSL sockets, otherwise the behavior is
  // undefined.
  virtual void GetSSLInfo(SSLInfo* ssl_info) = 0;

  // Returns true and populates |alternative_service| if an alternative service
  // was used to for this stream. Otherwise returns false.
  virtual bool GetAlternativeService(
      AlternativeService* alternative_service) const = 0;

  // Get the SSLCertRequestInfo associated with this stream's connection.
  // This should only be called for streams over SSL sockets, otherwise the
  // behavior is undefined.
  virtual void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) = 0;

  // Gets the remote endpoint of the socket that the HTTP stream is using, if
  // any. Returns true and fills in |endpoint| if it is available; returns false
  // and does not modify |endpoint| if it is unavailable.
  virtual bool GetRemoteEndpoint(IPEndPoint* endpoint) = 0;

  // In the case of an HTTP error or redirect, flush the response body (usually
  // a simple error or "this page has moved") so that we can re-use the
  // underlying connection. This stream is responsible for deleting itself when
  // draining is complete.
  virtual void Drain(HttpNetworkSession* session) = 0;

  // Get the network error details this stream is encountering.
  // Fills in |details| if it is available; leaves |details| unchanged if it
  // is unavailable.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) = 0;

  // Called when the priority of the parent transaction changes.
  virtual void SetPriority(RequestPriority priority) = 0;

  // Returns a new (not initialized) stream using the same underlying
  // connection and invalidates the old stream - no further methods should be
  // called on the old stream.  The caller should ensure that the response body
  // from the previous request is drained before calling this method.  If the
  // subclass does not support renewing the stream, NULL is returned.
  virtual HttpStream* RenewStreamForAuth() = 0;

  virtual void SetRequestHeadersCallback(RequestHeadersCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpStream);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_H_

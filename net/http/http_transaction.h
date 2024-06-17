// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_H_
#define NET_HTTP_HTTP_TRANSACTION_H_

#include <stdint.h>

#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/load_states.h"
#include "net/base/net_error_details.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/base/upload_progress.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/socket/connection_attempts.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

class AuthCredentials;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
struct TransportInfo;
struct LoadTimingInfo;
class NetLogWithSource;
class QuicServerInfo;
class SSLPrivateKey;
class X509Certificate;

// Represents a single HTTP transaction (i.e., a single request/response pair).
// HTTP redirects are not followed and authentication challenges are not
// answered.  Cookies are assumed to be managed by the caller.
class NET_EXPORT_PRIVATE HttpTransaction {
 public:
  // If |*defer| is set to true, the transaction will wait until
  // ResumeNetworkStart is called before establishing a connection.
  using BeforeNetworkStartCallback = base::OnceCallback<void(bool* defer)>;

  // Called each time a connection is obtained, before any data is sent.
  //
  // |info| describes the newly-obtained connection.
  //
  // This can be called multiple times for a single transaction, in the case of
  // retries, auth challenges, and split range requests.
  //
  // If this callback returns an error, the transaction fails with that error.
  // Otherwise the transaction continues unimpeded.
  // Must not return ERR_IO_PENDING.
  //
  // TODO(crbug.com/40637204): Fix handling of OnConnected() when proxy
  // authentication is required. We should notify this callback that a
  // connection was established, even though the stream might not be ready for
  // us to send data through it.
  using ConnectedCallback =
      base::RepeatingCallback<int(const TransportInfo& info,
                                  CompletionOnceCallback callback)>;

  // Stops any pending IO and destroys the transaction object.
  virtual ~HttpTransaction() = default;

  // Starts the HTTP transaction (i.e., sends the HTTP request).
  //
  // TODO(crbug.com/40521353) The consumer should ensure that request_info
  // points to a valid value till final response headers are received; after
  // that point, the HttpTransaction will not access |*request_info| and it may
  // be deleted.
  //
  // Returns OK if the transaction could be started synchronously, which means
  // that the request was served from the cache.  ERR_IO_PENDING is returned to
  // indicate that |callback| will be notified once response info is available
  // or if an IO error occurs.  Any other return value indicates that the
  // transaction could not be started.
  //
  // Regardless of the return value, the caller is expected to keep the
  // request_info object alive until Destroy is called on the transaction.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log) = 0;

  // Restarts the HTTP transaction, ignoring the last error.  This call can
  // only be made after a call to Start (or RestartIgnoringLastError) failed.
  // Once Read has been called, this method cannot be called.  This method is
  // used, for example, to continue past various SSL related errors.
  //
  // Not all errors can be ignored using this method.  See error code
  // descriptions for details about errors that can be ignored.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  //
  virtual int RestartIgnoringLastError(CompletionOnceCallback callback) = 0;

  // Restarts the HTTP transaction with a client certificate.
  virtual int RestartWithCertificate(
      scoped_refptr<X509Certificate> client_cert,
      scoped_refptr<SSLPrivateKey> client_private_key,
      CompletionOnceCallback callback) = 0;

  // Restarts the HTTP transaction with authentication credentials.
  virtual int RestartWithAuth(const AuthCredentials& credentials,
                              CompletionOnceCallback callback) = 0;

  // Returns true if auth is ready to be continued. Callers should check
  // this value anytime Start() completes: if it is true, the transaction
  // can be resumed with RestartWithAuth(L"", L"", callback) to resume
  // the automatic auth exchange. This notification gives the caller a
  // chance to process the response headers from all of the intermediate
  // restarts needed for authentication.
  virtual bool IsReadyToRestartForAuth() = 0;

  // Once response info is available for the transaction, response data may be
  // read by calling this method.
  //
  // Response data is copied into the given buffer and the number of bytes
  // copied is returned.  ERR_IO_PENDING is returned if response data is not yet
  // available.  |callback| is notified when the data copy completes, and it is
  // passed the number of bytes that were successfully copied.  Or, if a read
  // error occurs, |callback| is notified of the error.  Any other negative
  // return value indicates that the transaction could not be read.
  //
  // NOTE: The transaction is not responsible for deleting the callback object.
  // If the operation is not completed immediately, the transaction must acquire
  // a reference to the provided buffer.
  //
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   CompletionOnceCallback callback) = 0;

  // Stops further caching of this request by the HTTP cache, if there is any.
  // Note that this is merely a hint to the transaction which it may choose to
  // ignore.
  virtual void StopCaching() = 0;

  // Get the number of bytes received from network.
  virtual int64_t GetTotalReceivedBytes() const = 0;

  // Get the number of bytes sent over the network.
  virtual int64_t GetTotalSentBytes() const = 0;

  // Get the number of bytes of the body received from network.
  virtual int64_t GetReceivedBodyBytes() const = 0;

  // Called to tell the transaction that we have successfully reached the end
  // of the stream. This is equivalent to performing an extra Read() at the end
  // that should return 0 bytes. This method should not be called if the
  // transaction is busy processing a previous operation (like a pending Read).
  //
  // DoneReading may also be called before the first Read() to notify that the
  // entire response body is to be ignored (e.g., in a redirect).
  virtual void DoneReading() = 0;

  // Returns the response info for this transaction. Must not be called until
  // |Start| completes.
  virtual const HttpResponseInfo* GetResponseInfo() const = 0;

  // Returns the load state for this transaction.
  virtual LoadState GetLoadState() const = 0;

  // SetQuicServerInfo sets a object which reads and writes public information
  // about a QUIC server.
  virtual void SetQuicServerInfo(QuicServerInfo* quic_server_info) = 0;

  // Populates all of load timing, except for request start times and receive
  // headers time.
  // |load_timing_info| must have all null times when called.  Returns false and
  // does not modify |load_timing_info| if there's no timing information to
  // provide.
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const = 0;

  // Gets the remote endpoint of the socket that the transaction's underlying
  // stream is using or did use, if any. Returns true and fills in |endpoint|
  // if it is available; returns false and leaves |endpoint| unchanged if it is
  // unavailable.
  virtual bool GetRemoteEndpoint(IPEndPoint* endpoint) const = 0;

  // Populates network error details for this transaction.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) const = 0;

  // Called when the priority of the parent job changes.
  virtual void SetPriority(RequestPriority priority) = 0;

  // Set the WebSocketHandshakeStreamBase::CreateHelper to be used for the
  // request.  Only relevant to WebSocket transactions. Must be called before
  // Start(). Ownership of |create_helper| remains with the caller.
  virtual void SetWebSocketHandshakeStreamCreateHelper(
      WebSocketHandshakeStreamBase::CreateHelper* create_helper) = 0;

  // Sets the callback to receive notification just before network use.
  virtual void SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback callback) = 0;

  // Sets the callback to receive a notification upon connection.
  virtual void SetConnectedCallback(const ConnectedCallback& callback) = 0;

  virtual void SetRequestHeadersCallback(RequestHeadersCallback callback) = 0;
  virtual void SetEarlyResponseHeadersCallback(
      ResponseHeadersCallback callback) = 0;
  virtual void SetResponseHeadersCallback(ResponseHeadersCallback callback) = 0;

  // Sets the callback to modify the request header. The callback will be called
  // just before sending the request to the network.
  virtual void SetModifyRequestHeadersCallback(
      base::RepeatingCallback<void(HttpRequestHeaders*)> callback) = 0;

  virtual void SetIsSharedDictionaryReadAllowedCallback(
      base::RepeatingCallback<bool()> callback) = 0;

  // Resumes the transaction after being deferred.
  virtual int ResumeNetworkStart() = 0;

  virtual ConnectionAttempts GetConnectionAttempts() const = 0;

  // Configures the transaction to close the network connection, if any, on
  // destruction. Intended for cases where keeping the socket alive may leak
  // data. Does not immediately close the socket. If multiple transactions are
  // using the same socket, only closes it once all transactions have completed.
  //
  // Does not close H2/H3 sessions, but does close H1 tunnels on top of H2/H3
  // sessions.
  //
  // Only applies to currently in-use connections. Does nothing after the last
  // byte of the response body has been read, as the connection is no longer in
  // use at that point.
  virtual void CloseConnectionOnDestruction() = 0;

  // Returns true if ProxyInfo has been determined for the transaction and that
  // the ProxyInfo indicates the origin's domain is on the IP Protection Masked
  // Domain List. Note that this may not be determined if no network request is
  // actually made (and thus no ProxyInfo computed). However, the metrics we're
  // interested in focus on requests which actually reach out to the network, so
  // this is not a problem. See also HttpResponseInfo's was_mdl_match as a
  // secondary signal.
  //
  // Only use this method for metrics. It may be removed when associated
  // histograms are removed.
  virtual bool IsMdlMatchForMetrics() const = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_H_

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_BIDIRECTIONAL_STREAM_H_
#define NET_HTTP_BIDIRECTIONAL_STREAM_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class HttpAuthController;
class HttpNetworkSession;
class HttpStream;
class IOBuffer;
class ProxyInfo;
struct BidirectionalStreamRequestInfo;
struct NetErrorDetails;

// A class to do HTTP/2 bidirectional streaming. Note that at most one each of
// ReadData or SendData/SendvData should be in flight until the operation
// completes. The BidirectionalStream must be torn down before the
// HttpNetworkSession.
class NET_EXPORT BidirectionalStream : public BidirectionalStreamImpl::Delegate,
                                       public HttpStreamRequest::Delegate {
 public:
  // Delegate interface to get notified of success of failure. Callbacks will be
  // invoked asynchronously.
  class NET_EXPORT Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called when the stream is ready for writing and reading. This is called
    // at most once for the lifetime of a stream.
    // The delegate may call BidirectionalStream::ReadData to start reading,
    // or call BidirectionalStream::SendData to send data.
    // The delegate should not call BidirectionalStream::Cancel
    // during this callback.
    // |request_headers_sent| if true, request headers have been sent. If false,
    // SendRequestHeaders() needs to be explicitly called.
    virtual void OnStreamReady(bool request_headers_sent) = 0;

    // Called when headers are received. This is called at most once for the
    // lifetime of a stream.
    // The delegate may call BidirectionalStream::ReadData to start reading,
    // call BidirectionalStream::SendData to send data,
    // or call BidirectionalStream::Cancel to cancel the stream.
    virtual void OnHeadersReceived(
        const quiche::HttpHeaderBlock& response_headers) = 0;

    // Called when a pending read is completed asynchronously.
    // |bytes_read| specifies how much data is read.
    // The delegate may call BidirectionalStream::ReadData to continue
    // reading, call BidirectionalStream::SendData to send data,
    // or call BidirectionalStream::Cancel to cancel the stream.
    virtual void OnDataRead(int bytes_read) = 0;

    // Called when the entire buffer passed through SendData is sent.
    // The delegate may call BidirectionalStream::ReadData to continue
    // reading, call BidirectionalStream::SendData to send data,
    // The delegate should not call BidirectionalStream::Cancel
    // during this callback.
    virtual void OnDataSent() = 0;

    // Called when trailers are received. This is called as soon as trailers
    // are received, which can happen before a read completes.
    // The delegate is able to continue reading if there is no pending read and
    // EOF has not been received, or to send data if there is no pending send.
    virtual void OnTrailersReceived(
        const quiche::HttpHeaderBlock& trailers) = 0;

    // Called when an error occurred. Do not call into the stream after this
    // point. No other delegate functions will be called after this.
    virtual void OnFailed(int error) = 0;

   protected:
    virtual ~Delegate();
  };

  // Constructs a BidirectionalStream. |request_info| contains information about
  // the request, and must be non-NULL. |session| is the http network session
  // with which this request will be made. |delegate| must be non-NULL.
  // |session| and |delegate| must outlive |this|.
  // |send_request_headers_automatically| if true, request headers will be sent
  // automatically when stream is negotiated. If false, request headers will be
  // sent only when SendRequestHeaders() is invoked or with
  // next SendData/SendvData.
  BidirectionalStream(
      std::unique_ptr<BidirectionalStreamRequestInfo> request_info,
      HttpNetworkSession* session,
      bool send_request_headers_automatically,
      Delegate* delegate);

  // Constructor that accepts a Timer, which can be used in tests to control
  // the buffering of received data.
  BidirectionalStream(
      std::unique_ptr<BidirectionalStreamRequestInfo> request_info,
      HttpNetworkSession* session,
      bool send_request_headers_automatically,
      Delegate* delegate,
      std::unique_ptr<base::OneShotTimer> timer);

  BidirectionalStream(const BidirectionalStream&) = delete;
  BidirectionalStream& operator=(const BidirectionalStream&) = delete;

  // Cancels |stream_request_| or |stream_impl_| if applicable.
  // |this| should not be destroyed during Delegate::OnHeadersSent or
  // Delegate::OnDataSent.
  ~BidirectionalStream() override;

  // Sends request headers to server.
  // When |send_request_headers_automatically_| is
  // false and OnStreamReady() is invoked with request_headers_sent = false,
  // headers will be combined with next SendData/SendvData unless this
  // method is called first, in which case headers will be sent separately
  // without delay.
  // (This method cannot be called when |send_request_headers_automatically_| is
  // true nor when OnStreamReady() is invoked with request_headers_sent = true,
  // since headers have been sent by the stream when stream is negotiated
  // successfully.)
  void SendRequestHeaders();

  // Reads at most |buf_len| bytes into |buf|. Returns the number of bytes read,
  // or ERR_IO_PENDING if the read is to be completed asynchronously, or an
  // error code if any error occurred. If returns 0, there is no more data to
  // read. This should not be called before Delegate::OnStreamReady is
  // invoked, and should not be called again unless it returns with number
  // greater than 0 or until Delegate::OnDataRead is invoked.
  int ReadData(IOBuffer* buf, int buf_len);

  // Sends data. This should not be called before Delegate::OnStreamReady is
  // invoked, and should not be called again until Delegate::OnDataSent is
  // invoked. If |end_stream| is true, the DATA frame will have an END_STREAM
  // flag.
  void SendvData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                 const std::vector<int>& lengths,
                 bool end_stream);

  // Returns the protocol used by this stream. If stream has not been
  // established, return kProtoUnknown.
  NextProto GetProtocol() const;

  // Total number of bytes received over the network of SPDY data, headers, and
  // push_promise frames associated with this stream, including the size of
  // frame headers, after SSL decryption and not including proxy overhead.
  // If stream has not been established, return 0.
  int64_t GetTotalReceivedBytes() const;

  // Total number of bytes sent over the network of SPDY frames associated with
  // this stream, including the size of frame headers, before SSL encryption and
  // not including proxy overhead. Note that some SPDY frames such as pings are
  // not associated with any stream, and are not included in this value.
  int64_t GetTotalSentBytes() const;

  // Gets LoadTimingInfo of this stream.
  void GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  // Get the network error details this stream is encountering.
  // Fills in |details| if it is available; leaves |details| unchanged if it
  // is unavailable.
  void PopulateNetErrorDetails(NetErrorDetails* details);

 private:
  void StartRequest();
  // BidirectionalStreamImpl::Delegate implementation:
  void OnStreamReady(bool request_headers_sent) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataRead(int bytes_read) override;
  void OnDataSent() override;
  void OnTrailersReceived(const quiche::HttpHeaderBlock& trailers) override;
  void OnFailed(int error) override;

  // HttpStreamRequest::Delegate implementation:
  void OnStreamReady(const ProxyInfo& used_proxy_info,
                     std::unique_ptr<HttpStream> stream) override;
  void OnBidirectionalStreamImplReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<BidirectionalStreamImpl> stream) override;
  void OnWebSocketHandshakeStreamReady(
      const ProxyInfo& used_proxy_info,
      std::unique_ptr<WebSocketHandshakeStreamBase> stream) override;
  void OnStreamFailed(int status,
                      const NetErrorDetails& net_error_details,
                      const ProxyInfo& used_proxy_info,
                      ResolveErrorInfo resolve_error_info) override;
  void OnCertificateError(int status, const SSLInfo& ssl_info) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response_info,
                        const ProxyInfo& used_proxy_info,
                        HttpAuthController* auth_controller) override;
  void OnNeedsClientAuth(SSLCertRequestInfo* cert_info) override;
  void OnQuicBroken() override;
  void OnSwitchesToHttpStreamPool(
      HttpStreamPoolSwitchingInfo switching_info) override;

  // Helper method to notify delegate if there is an error.
  void NotifyFailed(int error);

  // BidirectionalStreamRequestInfo used when requesting the stream.
  std::unique_ptr<BidirectionalStreamRequestInfo> request_info_;
  const NetLogWithSource net_log_;

  raw_ptr<HttpNetworkSession> session_;

  bool send_request_headers_automatically_;
  // Whether request headers have been sent, as indicated in OnStreamReady()
  // callback.
  bool request_headers_sent_ = false;

  const raw_ptr<Delegate> delegate_;

  // Timer used to buffer data received in short time-spans and send a single
  // read completion notification.
  std::unique_ptr<base::OneShotTimer> timer_;
  // HttpStreamRequest used to request a BidirectionalStreamImpl. This is NULL
  // if the request has been canceled or completed.
  std::unique_ptr<HttpStreamRequest> stream_request_;
  // The underlying BidirectioanlStreamImpl used for this stream. It is
  // non-NULL, if the |stream_request_| successfully finishes.
  std::unique_ptr<BidirectionalStreamImpl> stream_impl_;

  // Buffer used for reading.
  scoped_refptr<IOBuffer> read_buffer_;
  // List of buffers used for writing.
  std::vector<scoped_refptr<IOBuffer>> write_buffer_list_;
  // List of buffer length.
  std::vector<int> write_buffer_len_list_;

  // TODO(xunjieli): Remove this once LoadTimingInfo has response end.
  base::TimeTicks read_end_time_;

  // Load timing info of this stream. |connect_timing| is obtained when headers
  // are received. Other fields are populated at different stages of the request
  LoadTimingInfo load_timing_info_;

  base::WeakPtrFactory<BidirectionalStream> weak_factory_{this};
};

}  // namespace net

#endif  // NET_HTTP_BIDIRECTIONAL_STREAM_H_

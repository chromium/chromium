// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_BIDIRECTIONAL_STREAM_IMPL_H_
#define NET_HTTP_BIDIRECTIONAL_STREAM_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class IOBuffer;
class NetLogWithSource;
struct BidirectionalStreamRequestInfo;
struct NetErrorDetails;

// Exposes an interface to do HTTP/2 bidirectional streaming.
// Note that only one ReadData or SendData should be in flight until the
// operation completes synchronously or asynchronously.
// BidirectionalStreamImpl once created by HttpStreamFactory should be owned
// by BidirectionalStream.
class NET_EXPORT_PRIVATE BidirectionalStreamImpl {
 public:
  // Delegate to handle BidirectionalStreamImpl events.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    // Called when the stream is ready for reading and writing.
    // The delegate may call BidirectionalStreamImpl::ReadData to start reading,
    // call BidirectionalStreamImpl::SendData to send data,
    // or call BidirectionalStreamImpl::Cancel to cancel the stream.
    // The delegate should not call BidirectionalStreamImpl::Cancel
    // during this callback.
    // |request_headers_sent| if true, request headers have been sent. If false,
    // SendRequestHeaders() needs to be explicitly called.
    virtual void OnStreamReady(bool request_headers_sent) = 0;

    // Called when response headers are received.
    // This is called at most once for the lifetime of a stream.
    // The delegate may call BidirectionalStreamImpl::ReadData to start
    // reading, call BidirectionalStreamImpl::SendData to send data,
    // or call BidirectionalStreamImpl::Cancel to cancel the stream.
    virtual void OnHeadersReceived(
        const quiche::HttpHeaderBlock& response_headers) = 0;

    // Called when read is completed asynchronously. |bytes_read| specifies how
    // much data is available.
    // The delegate may call BidirectionalStreamImpl::ReadData to continue
    // reading, call BidirectionalStreamImpl::SendData to send data,
    // or call BidirectionalStreamImpl::Cancel to cancel the stream.
    virtual void OnDataRead(int bytes_read) = 0;

    // Called when the entire buffer passed through SendData is sent.
    // The delegate may call BidirectionalStreamImpl::ReadData to continue
    // reading, or call BidirectionalStreamImpl::SendData to send data.
    // The delegate should not call BidirectionalStreamImpl::Cancel
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
    virtual void OnFailed(int status) = 0;

   protected:
    virtual ~Delegate();
  };

  BidirectionalStreamImpl();

  BidirectionalStreamImpl(const BidirectionalStreamImpl&) = delete;
  BidirectionalStreamImpl& operator=(const BidirectionalStreamImpl&) = delete;

  // |this| should not be destroyed during Delegate::OnHeadersSent or
  // Delegate::OnDataSent.
  virtual ~BidirectionalStreamImpl();

  // Starts the BidirectionalStreamImpl and sends request headers.
  // |send_request_headers_automatically| if true, request headers will be sent
  // automatically when stream is negotiated. If false, request headers will be
  // sent only when SendRequestHeaders() is invoked or with next
  // SendData/SendvData.
  virtual void Start(const BidirectionalStreamRequestInfo* request_info,
                     const NetLogWithSource& net_log,
                     bool send_request_headers_automatically,
                     BidirectionalStreamImpl::Delegate* delegate,
                     std::unique_ptr<base::OneShotTimer> timer,
                     const NetworkTrafficAnnotationTag& traffic_annotation) = 0;

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
  virtual void SendRequestHeaders() = 0;

  // Reads at most |buf_len| bytes into |buf|. Returns the number of bytes read,
  // ERR_IO_PENDING if the read is to be completed asynchronously, or an error
  // code if any error occurred. If returns 0, there is no more data to read.
  // This should not be called before Delegate::OnHeadersReceived is invoked,
  // and should not be called again unless it returns with number greater than
  // 0 or until Delegate::OnDataRead is invoked.
  virtual int ReadData(IOBuffer* buf, int buf_len) = 0;

  // Sends data. This should not be called be called before
  // Delegate::OnHeadersSent is invoked, and should not be called again until
  // Delegate::OnDataSent is invoked. If |end_stream| is true, the DATA frame
  // will have an END_STREAM flag.
  virtual void SendvData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                         const std::vector<int>& lengths,
                         bool end_stream) = 0;

  // Returns the protocol used by this stream. If stream has not been
  // established, return kProtoUnknown.
  virtual NextProto GetProtocol() const = 0;

  // Total number of bytes received over the network of SPDY data, headers, and
  // push_promise frames associated with this stream, including the size of
  // frame headers, after SSL decryption and not including proxy overhead.
  virtual int64_t GetTotalReceivedBytes() const = 0;

  // Total number of bytes sent over the network of SPDY frames associated with
  // this stream, including the size of frame headers, before SSL encryption and
  // not including proxy overhead. Note that some SPDY frames such as pings are
  // not associated with any stream, and are not included in this value.
  virtual int64_t GetTotalSentBytes() const = 0;

  // Populates the connection establishment part of |load_timing_info|, and
  // socket reuse info. Return true if LoadTimingInfo is obtained successfully
  // and false otherwise.
  virtual bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const = 0;

  // Get the network error details this stream is encountering.
  // Fills in |details| if it is available; leaves |details| unchanged if it
  // is unavailable.
  virtual void PopulateNetErrorDetails(NetErrorDetails* details) = 0;
};

}  // namespace net

#endif  // NET_HTTP_BIDIRECTIONAL_STREAM_IMPL_H_

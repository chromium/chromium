// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_IMPL_H_
#define NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_IMPL_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/bidirectional_stream_request_info.h"
#include "net/http/http_request_info.h"
#include "net/log/net_log_source.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace spdy {
class SpdyHeaderBlock;
}  // namespace spdy

namespace net {

class IOBuffer;
class NetLogWithSource;

class NET_EXPORT_PRIVATE BidirectionalStreamSpdyImpl
    : public BidirectionalStreamImpl,
      public SpdyStream::Delegate {
 public:
  BidirectionalStreamSpdyImpl(const base::WeakPtr<SpdySession>& spdy_session,
                              NetLogSource source_dependency);

  ~BidirectionalStreamSpdyImpl() override;

  // BidirectionalStreamImpl implementation:
  void Start(const BidirectionalStreamRequestInfo* request_info,
             const NetLogWithSource& net_log,
             bool send_request_headers_automatically,
             BidirectionalStreamImpl::Delegate* delegate,
             std::unique_ptr<base::OneShotTimer> timer,
             const NetworkTrafficAnnotationTag& traffic_annotation) override;
  void SendRequestHeaders() override;
  int ReadData(IOBuffer* buf, int buf_len) override;
  void SendvData(const std::vector<scoped_refptr<IOBuffer>>& buffers,
                 const std::vector<int>& lengths,
                 bool end_stream) override;
  NextProto GetProtocol() const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  void PopulateNetErrorDetails(NetErrorDetails* details) override;

  // SpdyStream::Delegate implementation:
  void OnHeadersSent() override;
  void OnHeadersReceived(
      const spdy::SpdyHeaderBlock& response_headers,
      const spdy::SpdyHeaderBlock* pushed_request_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const spdy::SpdyHeaderBlock& trailers) override;
  void OnClose(int status) override;
  NetLogSource source_dependency() const override;

 private:
  int SendRequestHeadersHelper();
  void OnStreamInitialized(int rv);
  // Notifies delegate of an error.
  void NotifyError(int rv);
  void ResetStream();
  void ScheduleBufferedRead();
  void DoBufferedRead();
  bool ShouldWaitForMoreBufferedData() const;
  // Handles the case where stream is closed when SendData()/SendvData() is
  // called. Return true if stream is closed.
  bool MaybeHandleStreamClosedInSendData();

  const base::WeakPtr<SpdySession> spdy_session_;
  const BidirectionalStreamRequestInfo* request_info_;
  BidirectionalStreamImpl::Delegate* delegate_;
  std::unique_ptr<base::OneShotTimer> timer_;
  SpdyStreamRequest stream_request_;
  base::WeakPtr<SpdyStream> stream_;
  const NetLogSource source_dependency_;

  NextProto negotiated_protocol_;

  // Buffers the data as it arrives asynchronously from the stream.
  SpdyReadQueue read_data_queue_;
  // Whether received more data has arrived since started waiting.
  bool more_read_data_pending_;
  // User provided read buffer for ReadData() response.
  scoped_refptr<IOBuffer> read_buffer_;
  int read_buffer_len_;

  // Whether client has written the end of stream flag in request headers or
  // in SendData()/SendvData().
  bool written_end_of_stream_;
  // Whether a SendData() or SendvData() is pending.
  bool write_pending_;

  // Whether OnClose has been invoked.
  bool stream_closed_;
  // Status reported in OnClose.
  int closed_stream_status_;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes received over the network for |stream_| while it was open.
  int64_t closed_stream_received_bytes_;
  // After |stream_| has been closed, this keeps track of the total number of
  // bytes sent over the network for |stream_| while it was open.
  int64_t closed_stream_sent_bytes_;
  // True if |stream_| has LoadTimingInfo when it is closed.
  bool closed_has_load_timing_info_;
  // LoadTimingInfo populated when |stream_| is closed.
  LoadTimingInfo closed_load_timing_info_;

  // This is the combined buffer of buffers passed in through SendvData.
  // Keep a reference here so it is alive until OnDataSent is invoked.
  scoped_refptr<IOBuffer> pending_combined_buffer_;

  base::WeakPtrFactory<BidirectionalStreamSpdyImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BidirectionalStreamSpdyImpl);
};

}  // namespace net

#endif  // NET_SPDY_BIDIRECTIONAL_STREAM_SPDY_IMPL_H_

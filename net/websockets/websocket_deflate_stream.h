// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_DEFLATE_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_DEFLATE_STREAM_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_inflater.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class WebSocketDeflateParameters;
class WebSocketDeflatePredictor;

// WebSocketDeflateStream is a WebSocketStream subclass.
// WebSocketDeflateStream is for permessage-deflate WebSocket extension[1].
//
// WebSocketDeflateStream::ReadFrames and WriteFrames may change frame
// boundary. In particular, if a control frame is placed in the middle of
// data message frames, the control frame can overtake data frames.
// Say there are frames df1, df2 and cf, df1 and df2 are frames of a
// data message and cf is a control message frame. cf may arrive first and
// data frames may follow cf.
// Note that message boundary will be preserved, i.e. if the last frame of
// a message m1 is read / written before the last frame of a message m2,
// WebSocketDeflateStream will respect the order.
//
// [1]: http://tools.ietf.org/html/draft-ietf-hybi-permessage-compression-12
class NET_EXPORT_PRIVATE WebSocketDeflateStream : public WebSocketStream {
 public:
  WebSocketDeflateStream(std::unique_ptr<WebSocketStream> stream,
                         const WebSocketDeflateParameters& params,
                         std::unique_ptr<WebSocketDeflatePredictor> predictor);
  ~WebSocketDeflateStream() override;

  // WebSocketStream functions.
  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override;
  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override;
  void Close() override;
  std::string GetSubProtocol() const override;
  std::string GetExtensions() const override;

 private:
  enum ReadingState {
    READING_COMPRESSED_MESSAGE,
    READING_UNCOMPRESSED_MESSAGE,
    NOT_READING,
  };

  enum WritingState {
    WRITING_COMPRESSED_MESSAGE,
    WRITING_UNCOMPRESSED_MESSAGE,
    WRITING_POSSIBLY_COMPRESSED_MESSAGE,
    NOT_WRITING,
  };

  // Handles asynchronous completion of ReadFrames() call on |stream_|.
  void OnReadComplete(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                      int result);

  // This function deflates |frames| and stores the result to |frames| itself.
  int Deflate(std::vector<std::unique_ptr<WebSocketFrame>>* frames);
  void OnMessageStart(
      const std::vector<std::unique_ptr<WebSocketFrame>>& frames,
      size_t index);
  int AppendCompressedFrame(
      const WebSocketFrameHeader& header,
      std::vector<std::unique_ptr<WebSocketFrame>>* frames_to_write);
  int AppendPossiblyCompressedMessage(
      std::vector<std::unique_ptr<WebSocketFrame>>* frames,
      std::vector<std::unique_ptr<WebSocketFrame>>* frames_to_write);

  // This function inflates |frames| and stores the result to |frames| itself.
  int Inflate(std::vector<std::unique_ptr<WebSocketFrame>>* frames);

  int InflateAndReadIfNecessary(
      std::vector<std::unique_ptr<WebSocketFrame>>* frames);

  const std::unique_ptr<WebSocketStream> stream_;
  WebSocketDeflater deflater_;
  WebSocketInflater inflater_;
  ReadingState reading_state_;
  WritingState writing_state_;
  WebSocketFrameHeader::OpCode current_reading_opcode_;
  WebSocketFrameHeader::OpCode current_writing_opcode_;
  std::unique_ptr<WebSocketDeflatePredictor> predictor_;

  // User callback saved for asynchronous reads.
  CompletionOnceCallback read_callback_;

  // References of Deflater outputs kept until next WriteFrames().
  std::vector<scoped_refptr<IOBufferWithSize>> deflater_outputs_;
  // References of Inflater outputs kept until next ReadFrames().
  std::vector<scoped_refptr<IOBufferWithSize>> inflater_outputs_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketDeflateStream);
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_DEFLATE_STREAM_H_

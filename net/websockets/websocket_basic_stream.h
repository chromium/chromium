// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_frame_parser.h"
#include "net/websockets/websocket_stream.h"

namespace net {

class ClientSocketHandle;
class DrainableIOBuffer;
class GrowableIOBuffer;
class IOBuffer;
class IOBufferWithSize;
struct WebSocketFrame;
struct WebSocketFrameChunk;

// Implementation of WebSocketStream for non-multiplexed ws:// connections (or
// the physical side of a multiplexed ws:// connection).
//
// Please update the traffic annotations in the websocket_basic_stream.cc and
// websocket_stream.cc if the class is used for any communication with Google.
// In such a case, annotation should be passed from the callers to this class
// and a local annotation can not be used anymore.
class NET_EXPORT_PRIVATE WebSocketBasicStream : public WebSocketStream {
 public:
  typedef WebSocketMaskingKey (*WebSocketMaskingKeyGeneratorFunction)();

  // Adapter that allows WebSocketBasicStream to use
  // either a TCP/IP or TLS socket, or an HTTP/2 stream.
  class Adapter {
   public:
    virtual ~Adapter() = default;
    virtual int Read(IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) = 0;
    virtual int Write(
        IOBuffer* buf,
        int buf_len,
        CompletionOnceCallback callback,
        const NetworkTrafficAnnotationTag& traffic_annotation) = 0;
    virtual void Disconnect() = 0;
    virtual bool is_initialized() const = 0;
  };

  // This class should not normally be constructed directly; see
  // WebSocketStream::CreateAndConnectStream() and
  // WebSocketBasicHandshakeStream::Upgrade().
  WebSocketBasicStream(std::unique_ptr<Adapter> connection,
                       const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
                       const std::string& sub_protocol,
                       const std::string& extensions);

  // The destructor has to make sure the connection is closed when we finish so
  // that it does not get returned to the pool.
  ~WebSocketBasicStream() override;

  // WebSocketStream implementation.
  int ReadFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                 CompletionOnceCallback callback) override;

  int WriteFrames(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                  CompletionOnceCallback callback) override;

  void Close() override;

  std::string GetSubProtocol() const override;

  std::string GetExtensions() const override;

  ////////////////////////////////////////////////////////////////////////////
  // Methods for testing only.

  static std::unique_ptr<WebSocketBasicStream>
  CreateWebSocketBasicStreamForTesting(
      std::unique_ptr<ClientSocketHandle> connection,
      const scoped_refptr<GrowableIOBuffer>& http_read_buffer,
      const std::string& sub_protocol,
      const std::string& extensions,
      WebSocketMaskingKeyGeneratorFunction key_generator_function);

 private:
  // Reads until socket read returns asynchronously or returns error.
  // If returns ERR_IO_PENDING, then |read_callback_| will be called with result
  // later.
  int ReadEverything(std::vector<std::unique_ptr<WebSocketFrame>>* frames);

  // Called when a read completes. Parses the result, tries to read more.
  // Might call |read_callback_|.
  void OnReadComplete(std::vector<std::unique_ptr<WebSocketFrame>>* frames,
                      int result);

  // Writes until |buffer| is fully drained (in which case returns OK) or a
  // socket write returns asynchronously or returns an error.  If returns
  // ERR_IO_PENDING, then |write_callback_| will be called with result later.
  int WriteEverything(const scoped_refptr<DrainableIOBuffer>& buffer);

  // Called when a write completes.  Tries to write more.
  // Might call |write_callback_|.
  void OnWriteComplete(const scoped_refptr<DrainableIOBuffer>& buffer,
                       int result);

  // Attempts to parse the output of a read as WebSocket frames. On success,
  // returns OK and places the frame(s) in |frames|.
  int HandleReadResult(int result,
                       std::vector<std::unique_ptr<WebSocketFrame>>* frames);

  // Converts the chunks in |frame_chunks| into frames and writes them to
  // |frames|. |frame_chunks| is destroyed in the process. Returns
  // ERR_WS_PROTOCOL_ERROR if an invalid chunk was found. If one or more frames
  // was added to |frames|, then returns OK, otherwise returns ERR_IO_PENDING.
  int ConvertChunksToFrames(
      std::vector<std::unique_ptr<WebSocketFrameChunk>>* frame_chunks,
      std::vector<std::unique_ptr<WebSocketFrame>>* frames);

  // Converts a |chunk| to a |frame|. |*frame| should be NULL on entry to this
  // method. If |chunk| is an incomplete control frame, or an empty middle
  // frame, then |*frame| may still be NULL on exit. If an invalid control frame
  // is found, returns ERR_WS_PROTOCOL_ERROR and the stream is no longer
  // usable. Otherwise returns OK (even if frame is still NULL).
  int ConvertChunkToFrame(std::unique_ptr<WebSocketFrameChunk> chunk,
                          std::unique_ptr<WebSocketFrame>* frame);

  // Creates a frame based on the value of |is_final_chunk|, |data| and
  // |current_frame_header_|. Clears |current_frame_header_| if |is_final_chunk|
  // is true. |data| may be NULL if the frame has an empty payload. A frame in
  // the middle of a message with no data is not useful; in this case the
  // returned frame will be NULL. Otherwise, |current_frame_header_->opcode| is
  // set to Continuation after use if it was Text or Binary, in accordance with
  // WebSocket RFC6455 section 5.4.
  std::unique_ptr<WebSocketFrame> CreateFrame(bool is_final_chunk,
                                              base::span<const char> data);

  // Adds |data_buffer| to the end of |incomplete_control_frame_body_|, applying
  // bounds checks.
  void AddToIncompleteControlFrameBody(base::span<const char> data);

  // Storage for pending reads.
  scoped_refptr<IOBufferWithSize> read_buffer_;

  // The connection, wrapped in a ClientSocketHandle so that we can prevent it
  // from being returned to the pool.
  std::unique_ptr<Adapter> connection_;

  // Frame header for the frame currently being received. Only non-NULL while we
  // are processing the frame. If the frame arrives in multiple chunks, it can
  // remain non-NULL until additional chunks arrive. If the header of the frame
  // was invalid, this is set to NULL, the channel is failed, and subsequent
  // chunks of the same frame will be ignored.
  std::unique_ptr<WebSocketFrameHeader> current_frame_header_;

  // Although it should rarely happen in practice, a control frame can arrive
  // broken into chunks. This variable provides storage for a partial control
  // frame until the rest arrives. It will be empty the rest of the time.
  std::vector<char> incomplete_control_frame_body_;
  // Storage for payload of combined (see |incomplete_control_frame_body_|)
  // control frame.
  std::vector<char> complete_control_frame_body_;

  // Only used during handshake. Some data may be left in this buffer after the
  // handshake, in which case it will be picked up during the first call to
  // ReadFrames(). The type is GrowableIOBuffer for compatibility with
  // net::HttpStreamParser, which is used to parse the handshake.
  scoped_refptr<GrowableIOBuffer> http_read_buffer_;
  // Flag to keep above buffer until next ReadFrames() after decoding.
  bool is_http_read_buffer_decoded_ = false;

  // This keeps the current parse state (including any incomplete headers) and
  // parses frames.
  WebSocketFrameParser parser_;

  // The negotated sub-protocol, or empty for none.
  const std::string sub_protocol_;

  // The extensions negotiated with the remote server.
  const std::string extensions_;

  // This can be overridden in tests to make the output deterministic. We don't
  // use a Callback here because a function pointer is faster and good enough
  // for our purposes.
  WebSocketMaskingKeyGeneratorFunction generate_websocket_masking_key_;

  // User callback saved for asynchronous writes and reads.
  CompletionOnceCallback write_callback_;
  CompletionOnceCallback read_callback_;
};

NET_EXPORT extern const char kWebSocketReadBufferSize[];

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_H_

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_CHANNEL_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_CHANNEL_IMPL_H_

#include <stdint.h>
#include <memory>
#include <utility>
#include "base/memory/scoped_refptr.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/platform/web_callbacks.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BaseFetchContext;
class WebSocketChannelClient;
class WebSocketHandshakeThrottle;

// This is an implementation of WebSocketChannel. This is created on the main
// thread for Document, or on the worker thread for WorkerGlobalScope. All
// functions must be called on the execution context's thread.
class MODULES_EXPORT WebSocketChannelImpl final
    : public WebSocketChannel,
      public WebSocketHandleClient,
      public WebCallbacks<void, const WebString&> {
 public:
  // You can specify the source file and the line number information
  // explicitly by passing the last parameter.
  // In the usual case, they are set automatically and you don't have to
  // pass it.
  static WebSocketChannelImpl* Create(ExecutionContext* context,
                                      WebSocketChannelClient* client,
                                      std::unique_ptr<SourceLocation> location);
  static WebSocketChannelImpl* CreateForTesting(
      Document*,
      WebSocketChannelClient*,
      std::unique_ptr<SourceLocation>,
      WebSocketHandle*,
      std::unique_ptr<WebSocketHandshakeThrottle>);

  ~WebSocketChannelImpl() override;

  // Allows the caller to provide the Mojo pipe through which the socket is
  // connected, overriding the interface provider of the Document.
  bool Connect(const KURL&,
               const String& protocol,
               network::mojom::blink::WebSocketPtr);

  // WebSocketChannel functions.
  bool Connect(const KURL&, const String& protocol) override;
  void Send(const CString& message) override;
  void Send(const DOMArrayBuffer&,
            unsigned byte_offset,
            unsigned byte_length) override;
  void Send(scoped_refptr<BlobDataHandle>) override;
  void SendTextAsCharVector(std::unique_ptr<Vector<char>> data) override;
  void SendBinaryAsCharVector(std::unique_ptr<Vector<char>> data) override;
  // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
  // argument to omit payload.
  void Close(int code, const String& reason) override;
  void Fail(const String& reason,
            MessageLevel,
            std::unique_ptr<SourceLocation>) override;
  void Disconnect() override;

  void Trace(blink::Visitor*) override;

 private:
  class BlobLoader;
  class Message;
  struct ConnectInfo;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeBlob,
    kMessageTypeArrayBuffer,
    kMessageTypeTextAsCharVector,
    kMessageTypeBinaryAsCharVector,
    kMessageTypeClose,
  };

  struct ReceivedMessage {
    bool is_message_text;
    Vector<char> data;
  };

  WebSocketChannelImpl(ExecutionContext*,
                       WebSocketChannelClient*,
                       std::unique_ptr<SourceLocation>,
                       std::unique_ptr<WebSocketHandle>);

  void SendInternal(WebSocketHandle::MessageType,
                    const char* data,
                    wtf_size_t total_size,
                    uint64_t* consumed_buffered_amount);
  void ProcessSendQueue();
  void FlowControlIfNecessary();
  void InitialFlowControl();
  void FailAsError(const String& reason) {
    Fail(reason, kErrorMessageLevel, location_at_construction_->Clone());
  }
  void AbortAsyncOperations();
  void HandleDidClose(bool was_clean,
                      unsigned short code,
                      const String& reason);

  // WebSocketHandleClient functions.
  void DidConnect(WebSocketHandle*,
                  const String& selected_protocol,
                  const String& extensions) override;
  void DidStartOpeningHandshake(
      WebSocketHandle*,
      network::mojom::blink::WebSocketHandshakeRequestPtr) override;
  void DidFinishOpeningHandshake(
      WebSocketHandle*,
      network::mojom::blink::WebSocketHandshakeResponsePtr) override;
  void DidFail(WebSocketHandle*, const String& message) override;
  void DidReceiveData(WebSocketHandle*,
                      bool fin,
                      WebSocketHandle::MessageType,
                      const char* data,
                      size_t) override;
  void DidClose(WebSocketHandle*,
                bool was_clean,
                unsigned short code,
                const String& reason) override;
  void DidReceiveFlowControl(WebSocketHandle*, int64_t quota) override;
  void DidStartClosingHandshake(WebSocketHandle*) override;

  // WebCallbacks<void, const WebString&> functions. These are called with the
  // results of throttling.
  void OnSuccess() override;
  void OnError(const WebString& console_message) override;

  // Methods for BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileError::ErrorCode);

  void TearDownFailedConnection();
  bool ShouldDisallowConnection(const KURL&);

  BaseFetchContext* GetBaseFetchContext() const;

  // |handle_| is a handle of the connection.
  // |handle_| == nullptr means this channel is closed.
  std::unique_ptr<WebSocketHandle> handle_;

  // |client_| can be deleted while this channel is alive, but this class
  // expects that disconnect() is called before the deletion.
  Member<WebSocketChannelClient> client_;
  KURL url_;
  unsigned long identifier_;
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;
  Vector<char> receiving_message_data_;
  Member<ExecutionContext> execution_context_;

  bool receiving_message_type_is_text_;
  uint64_t sending_quota_;
  uint64_t received_data_size_for_flow_control_;
  wtf_size_t sent_size_of_top_message_;
  std::unique_ptr<FrameScheduler::ActiveConnectionHandle>
      connection_handle_for_scheduler_;

  std::unique_ptr<SourceLocation> location_at_construction_;
  network::mojom::blink::WebSocketHandshakeRequestPtr handshake_request_;
  std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle_;
  // This field is only initialised if the object is still waiting for a
  // throttle response when DidConnect is called.
  std::unique_ptr<ConnectInfo> connect_info_;
  bool throttle_passed_;

  static const uint64_t kReceivedDataSizeForFlowControlHighWaterMark = 1 << 15;
};

std::ostream& operator<<(std::ostream&, const WebSocketChannelImpl*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_CHANNEL_IMPL_H_

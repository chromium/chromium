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
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/websocket.mojom-blink.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel.h"
#include "third_party/blink/renderer/modules/websockets/websocket_message_chunk_accumulator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class BaseFetchContext;
enum class FileErrorCode;
class WebSocketChannelClient;
class WebSocketHandshakeThrottle;

// This is an implementation of WebSocketChannel. This is created on the main
// thread for Document, or on the worker thread for WorkerGlobalScope. All
// functions must be called on the execution context's thread.
class MODULES_EXPORT WebSocketChannelImpl final
    : public WebSocketChannel,
      public network::mojom::blink::WebSocketHandshakeClient,
      public network::mojom::blink::WebSocketClient {
  USING_PRE_FINALIZER(WebSocketChannelImpl, Dispose);

 public:
  // You can specify the source file and the line number information
  // explicitly by passing the last parameter.
  // In the usual case, they are set automatically and you don't have to
  // pass it.
  static WebSocketChannelImpl* Create(ExecutionContext* context,
                                      WebSocketChannelClient* client,
                                      std::unique_ptr<SourceLocation> location);
  static WebSocketChannelImpl* CreateForTesting(
      ExecutionContext*,
      WebSocketChannelClient*,
      std::unique_ptr<SourceLocation>,
      std::unique_ptr<WebSocketHandshakeThrottle>);

  WebSocketChannelImpl(ExecutionContext*,
                       WebSocketChannelClient*,
                       std::unique_ptr<SourceLocation>);
  ~WebSocketChannelImpl() override;

  // WebSocketChannel functions.
  bool Connect(const KURL&, const String& protocol) override;
  SendResult Send(const std::string& message,
                  base::OnceClosure completion_callback) override;
  SendResult Send(const DOMArrayBuffer&,
                  unsigned byte_offset,
                  unsigned byte_length,
                  base::OnceClosure completion_callback) override;
  void Send(scoped_refptr<BlobDataHandle>) override;
  // Start closing handshake. Use the CloseEventCodeNotSpecified for the code
  // argument to omit payload.
  void Close(int code, const String& reason) override;
  void Fail(const String& reason,
            mojom::ConsoleMessageLevel,
            std::unique_ptr<SourceLocation>) override;
  void Disconnect() override;
  void CancelHandshake() override;
  void ApplyBackpressure() override;
  void RemoveBackpressure() override;

  // network::mojom::blink::WebSocketHandshakeClient methods:
  void OnOpeningHandshakeStarted(
      network::mojom::blink::WebSocketHandshakeRequestPtr) override;
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::blink::WebSocket> websocket,
      mojo::PendingReceiver<network::mojom::blink::WebSocketClient>
          client_receiver,
      network::mojom::blink::WebSocketHandshakeResponsePtr,
      mojo::ScopedDataPipeConsumerHandle readable) override;

  // network::mojom::blink::WebSocketClient methods:
  void OnDataFrame(bool fin,
                   network::mojom::blink::WebSocketMessageType,
                   uint64_t data_length) override;
  void AddSendFlowControlQuota(int64_t quota) override;
  void OnDropChannel(bool was_clean,
                     uint16_t code,
                     const String& reason) override;
  void OnClosingHandshake() override;

  ExecutionContext* GetExecutionContext();

  void Trace(blink::Visitor*) override;

 private:
  struct DataFrame final {
    DataFrame(bool fin,
              network::mojom::blink::WebSocketMessageType type,
              uint32_t data_length)
        : fin(fin), type(type), data_length(data_length) {}

    bool fin;
    network::mojom::blink::WebSocketMessageType type;
    uint32_t data_length;
  };

  friend class WebSocketChannelImplHandshakeThrottleTest;
  FRIEND_TEST_ALL_PREFIXES(WebSocketChannelImplHandshakeThrottleTest,
                           ThrottleSucceedsFirst);
  FRIEND_TEST_ALL_PREFIXES(WebSocketChannelImplHandshakeThrottleTest,
                           HandshakeSucceedsFirst);
  FRIEND_TEST_ALL_PREFIXES(WebSocketChannelImplHandshakeThrottleTest,
                           ThrottleReportsErrorBeforeConnect);
  FRIEND_TEST_ALL_PREFIXES(WebSocketChannelImplHandshakeThrottleTest,
                           ThrottleReportsErrorAfterConnect);

  class BlobLoader;
  class Message;
  struct ConnectInfo;

  enum MessageType {
    kMessageTypeText,
    kMessageTypeBlob,
    kMessageTypeArrayBuffer,
    kMessageTypeClose,
  };

  struct ReceivedMessage {
    bool is_message_text;
    Vector<char> data;
  };

  // The state is defined to see the conceptual state more clearly than checking
  // various members (for DCHECKs for example). This is only used internally.
  enum class State {
    // The channel is running an opening handshake. This is the initial state.
    // It becomes |kOpen| when the connection is established. It becomes
    // |kDisconnected| when detecting an error.
    kConnecting,
    // The channel is ready to send / receive messages. It becomes
    // |kDisconnected| when the connection is closed or when an error happens.
    kOpen,
    // The channel is not ready for communication. The channel stays in this
    // state forever.
    kDisconnected,
  };
  State GetState() const;

  void SendInternal(network::mojom::blink::WebSocketMessageType,
                    const char* data,
                    wtf_size_t total_size,
                    uint64_t* consumed_buffered_amount);
  void SendAndAdjustQuota(bool final,
                          network::mojom::blink::WebSocketMessageType,
                          base::span<const char>,
                          uint64_t* consumed_buffered_amount);
  bool MaybeSendSynchronously(network::mojom::blink::WebSocketMessageType,
                              base::span<const char>);
  void ProcessSendQueue();
  void FailAsError(const String& reason) {
    Fail(reason, mojom::ConsoleMessageLevel::kError,
         location_at_construction_->Clone());
  }
  void AbortAsyncOperations();
  void HandleDidClose(bool was_clean, uint16_t code, const String& reason);

  // Completion callback. It is called with the results of throttling.
  void OnCompletion(const base::Optional<WebString>& error);

  // Methods for BlobLoader.
  void DidFinishLoadingBlob(DOMArrayBuffer*);
  void DidFailLoadingBlob(FileErrorCode);

  void TearDownFailedConnection();
  bool ShouldDisallowConnection(const KURL&);

  BaseFetchContext* GetBaseFetchContext() const;

  // Called when |readable_| becomes readable.
  void OnReadable(MojoResult result, const mojo::HandleSignalsState& state);
  void ConsumePendingDataFrames();
  void ConsumeDataFrame(bool fin,
                        network::mojom::blink::WebSocketMessageType type,
                        const char* data,
                        size_t data_size);
  String GetTextMessage(const Vector<base::span<const char>>& chunks,
                        wtf_size_t size);
  void OnConnectionError(const base::Location& set_from,
                         uint32_t custom_reason,
                         const std::string& description);
  void Dispose();

  const Member<WebSocketChannelClient> client_;
  KURL url_;
  uint64_t identifier_;
  Member<BlobLoader> blob_loader_;
  HeapDeque<Member<Message>> messages_;
  WebSocketMessageChunkAccumulator message_chunks_;
  const Member<ExecutionContext> execution_context_;

  bool backpressure_ = false;
  bool receiving_message_type_is_text_ = false;
  bool received_text_is_all_ascii_ = true;
  bool throttle_passed_ = false;
  bool has_initiated_opening_handshake_ = false;
  uint64_t sending_quota_ = 0;
  wtf_size_t sent_size_of_top_message_ = 0;
  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  const std::unique_ptr<const SourceLocation> location_at_construction_;
  network::mojom::blink::WebSocketHandshakeRequestPtr handshake_request_;
  std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle_;
  // This field is only initialised if the object is still waiting for a
  // throttle response when DidConnect is called.
  std::unique_ptr<ConnectInfo> connect_info_;

  mojo::Remote<network::mojom::blink::WebSocket> websocket_;
  mojo::Receiver<network::mojom::blink::WebSocketHandshakeClient>
      handshake_client_receiver_{this};
  mojo::Receiver<network::mojom::blink::WebSocketClient> client_receiver_;

  mojo::ScopedDataPipeConsumerHandle readable_;
  mojo::SimpleWatcher readable_watcher_;
  WTF::Deque<DataFrame> pending_data_frames_;

  const scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;
};

MODULES_EXPORT std::ostream& operator<<(std::ostream&,
                                        const WebSocketChannelImpl*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBSOCKETS_WEBSOCKET_CHANNEL_IMPL_H_

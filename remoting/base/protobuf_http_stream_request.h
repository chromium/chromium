// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_STREAM_REQUEST_H_
#define REMOTING_BASE_PROTOBUF_HTTP_STREAM_REQUEST_H_

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "remoting/base/protobuf_http_request_base.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace remoting {

class ProtobufHttpClient;
class ProtobufHttpStatus;
class ProtobufHttpStreamParser;

// A server streaming request.
class ProtobufHttpStreamRequest final
    : public ProtobufHttpRequestBase,
      public network::SimpleURLLoaderStreamConsumer {
 public:
  template <typename MessageType>
  using MessageCallback =
      base::RepeatingCallback<void(std::unique_ptr<MessageType> message)>;
  using StreamClosedCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status)>;

  static constexpr base::TimeDelta kStreamReadyTimeoutDuration =
      base::Seconds(30);

  explicit ProtobufHttpStreamRequest(
      std::unique_ptr<ProtobufHttpRequestConfig> config);
  ~ProtobufHttpStreamRequest() override;

  // Sets a callback that gets called when the stream is ready to receive data.
  void SetStreamReadyCallback(base::OnceClosure callback);

  // Sets a callback that gets called when the stream is closed.
  void SetStreamClosedCallback(StreamClosedCallback callback);

  // Sets the callback to be called every time a new message is received.
  // |MessageType| needs to be a protobuf message type.
  template <typename MessageType>
  void SetMessageCallback(const MessageCallback<MessageType>& callback) {
    default_message_ = &MessageType::default_instance();
    message_callback_ = base::BindRepeating(
        [](MessageCallback<MessageType> callback,
           std::unique_ptr<google::protobuf::MessageLite> generic_message) {
          std::move(callback).Run(std::unique_ptr<MessageType>(
              static_cast<MessageType*>(generic_message.release())));
        },
        callback);
  }

 private:
  friend class ProtobufHttpClient;

  // ProtobufHttpStreamParser callbacks.
  void OnMessage(const std::string& message);
  void OnStreamClosed(const ProtobufHttpStatus& status);

  // ProtobufHttpRequestBase implementations.
  void OnAuthFailed(const ProtobufHttpStatus& status) override;
  void StartRequestInternal(
      network::mojom::URLLoaderFactory* loader_factory) override;
  base::TimeDelta GetRequestTimeoutDuration() const override;

  // network::SimpleURLLoaderStreamConsumer implementations.
  void OnDataReceived(std::string_view string_view,
                      base::OnceClosure resume) override;
  void OnComplete(bool success) override;
  void OnRetry(base::OnceClosure start_retry) override;

  void OnStreamReadyTimeout();

  // Used to create new response message instances.
  raw_ptr<const google::protobuf::MessageLite> default_message_;

  std::unique_ptr<ProtobufHttpStreamParser> stream_parser_;

  base::OneShotTimer stream_ready_timeout_timer_;

  base::OnceClosure stream_ready_callback_;
  StreamClosedCallback stream_closed_callback_;
  base::RepeatingCallback<void(std::unique_ptr<google::protobuf::MessageLite>)>
      message_callback_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_STREAM_REQUEST_H_

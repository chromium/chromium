// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_
#define REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "remoting/base/protobuf_http_request_base.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace remoting {

// A simple unary request.
class ProtobufHttpRequest final : public ProtobufHttpRequestBase {
 public:
  template <typename ResponseType>
  using ResponseCallback =
      base::OnceCallback<void(const ProtobufHttpStatus& status,
                              std::unique_ptr<ResponseType> response)>;

  explicit ProtobufHttpRequest(
      std::unique_ptr<ProtobufHttpRequestConfig> config);
  ~ProtobufHttpRequest() override;

  // Sets the amount of time to wait before giving up on a given network request
  // and considering it an error. The default value is 30s. Set it to zero to
  // disable timeout.
  void SetTimeoutDuration(base::TimeDelta timeout_duration);

  // Sets the response callback. |ResponseType| needs to be a protobuf message
  // type.
  template <typename ResponseType>
  void SetResponseCallback(ResponseCallback<ResponseType> callback) {
    auto response = std::make_unique<ResponseType>();
    response_message_ = response.get();
    response_callback_ = base::BindOnce(
        [](std::unique_ptr<ResponseType> response,
           ResponseCallback<ResponseType> callback,
           const ProtobufHttpStatus& status) {
          if (!status.ok()) {
            response.reset();
          }
          std::move(callback).Run(status, std::move(response));
        },
        std::move(response), std::move(callback));
  }

 private:
  // ProtobufHttpRequestBase implementations.
  void OnAuthFailed(const ProtobufHttpStatus& status) override;
  void StartRequestInternal(
      network::mojom::URLLoaderFactory* loader_factory) override;
  base::TimeDelta GetRequestTimeoutDuration() const override;

  void OnResponse(std::unique_ptr<std::string> response_body);

  // Parses |response_body| and writes it to |response_message_|.
  ProtobufHttpStatus ParseResponse(std::unique_ptr<std::string> response_body);

  void RunResponseCallback(const ProtobufHttpStatus& status);

  base::TimeDelta timeout_duration_ = base::Seconds(30);
  base::OnceCallback<void(const ProtobufHttpStatus&)> response_callback_;

  // This is owned by |response_callback_|.
  raw_ptr<google::protobuf::MessageLite> response_message_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOBUF_HTTP_REQUEST_H_

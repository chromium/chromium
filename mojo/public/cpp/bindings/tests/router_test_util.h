// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_

#include <stdint.h>

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace test {

class MessageQueue;

void AllocRequestMessage(uint32_t name, const char* text, Message* message);
void AllocResponseMessage(uint32_t name,
                          const char* text,
                          uint64_t request_id,
                          Message* message);

class MessageAccumulator : public MessageReceiver {
 public:
  MessageAccumulator(MessageQueue* queue,
                     base::OnceClosure closure = base::OnceClosure());
  ~MessageAccumulator() override;

  bool Accept(Message* message) override;

 private:
  raw_ptr<MessageQueue> queue_;
  base::OnceClosure closure_;
};

class ResponseGenerator : public MessageReceiverWithResponderStatus {
 public:
  ResponseGenerator();

  bool Accept(Message* message) override;

  bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) override;
  bool SendResponse(uint32_t name,
                    uint64_t request_id,
                    const char* request_string,
                    MessageReceiver* responder);
};

class LazyResponseGenerator : public ResponseGenerator {
 public:
  explicit LazyResponseGenerator(
      base::OnceClosure closure = base::OnceClosure());

  ~LazyResponseGenerator() override;

  bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) override;

  bool has_responder() const { return !!responder_; }

  bool responder_is_valid() const { return responder_->IsConnected(); }

  void set_closure(base::OnceClosure closure) { closure_ = std::move(closure); }

  // Sends the response and delete the responder.
  void CompleteWithResponse() { Complete(true); }

  // Deletes the responder without sending a response.
  void CompleteWithoutResponse() { Complete(false); }

 private:
  // Completes the request handling by deleting responder_. Optionally
  // also sends a response.
  void Complete(bool send_response);

  std::unique_ptr<MessageReceiverWithStatus> responder_;
  uint32_t name_;
  uint64_t request_id_;
  std::string request_string_;
  base::OnceClosure closure_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_TESTS_ROUTER_TEST_UTIL_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/inspector_protocol/crdtp/cbor.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"

namespace auction_worklet {

namespace {

crdtp::span<uint8_t> ToSpan(const mojo_base::BigBuffer& buffer) {
  return crdtp::span<uint8_t>(buffer.data(), buffer.size());
}

crdtp::span<uint8_t> ToSpan(const std::string& string) {
  return crdtp::span<uint8_t>(reinterpret_cast<const uint8_t*>(string.data()),
                              string.size());
}

std::string ToString(const std::vector<uint8_t>& vector) {
  return std::string(reinterpret_cast<const char*>(vector.data()),
                     vector.size());
}

std::string ToString(crdtp::span<uint8_t> span) {
  return std::string(reinterpret_cast<const char*>(span.data()), span.size());
}

}  // namespace

TestDevToolsAgentClient::TestDevToolsAgentClient(
    mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent,
    std::string session_id,
    bool use_binary_protocol)
    : session_id_(std::move(session_id)),
      use_binary_protocol_(use_binary_protocol),
      agent_(std::move(agent)),
      receiver_(this) {
  agent_->AttachDevToolsSession(receiver_.BindNewEndpointAndPassRemote(),
                                session_.BindNewEndpointAndPassReceiver(),
                                io_session_.BindNewPipeAndPassReceiver(),
                                nullptr, use_binary_protocol_,
                                /*client_is_trusted=*/true, session_id_,
                                /*session_waits_for_debugger=*/false);
}

TestDevToolsAgentClient::~TestDevToolsAgentClient() = default;

void TestDevToolsAgentClient::RunCommand(Channel channel,
                                         int call_id,
                                         std::string method,
                                         std::string payload) {
  base::span<const uint8_t> message;
  std::vector<uint8_t> cbor;
  if (use_binary_protocol_) {
    // JSON -> CBOR.
    crdtp::Status status =
        crdtp::json::ConvertJSONToCBOR(ToSpan(payload), &cbor);
    CHECK(status.ok()) << status.Message();
    message = base::span<const uint8_t>(cbor.data(), cbor.size());
  } else {
    // Keep it JSON.
    message = base::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
  }

  if (channel == Channel::kMain)
    session_->DispatchProtocolCommand(call_id, method, message);
  else
    io_session_->DispatchProtocolCommand(call_id, method, message);
}

TestDevToolsAgentClient::Event
TestDevToolsAgentClient::RunCommandAndWaitForResult(Channel channel,
                                                    int call_id,
                                                    std::string method,
                                                    std::string payload) {
  RunCommand(channel, call_id, std::move(method), std::move(payload));
  return WaitForEvent(base::BindRepeating(
      [](int call_id, const Event& event) {
        return event.type == Event::Type::kResponse &&
               event.call_id == call_id && event.value.is_dict() &&
               event.value.GetDict().FindInt("id") == call_id;
      },
      call_id));
}

TestDevToolsAgentClient::Event TestDevToolsAgentClient::WaitForEvent(
    const EventPredicate& predicate) {
  while (true) {
    while (!events_.empty()) {
      Event event = std::move(*events_.begin());
      events_.pop_front();
      if (predicate.Run(event))
        return event;
    }
    // Note that this can't be using TaskEnvironment::RunUntilIdle() since it
    // can be used when the V8 thread may be sleeping inside
    // DebugCommandQueue::PauseForDebuggerAndRunCommands().
    base::RunLoop().RunUntilIdle();
  }
}

TestDevToolsAgentClient::Event
TestDevToolsAgentClient::WaitForMethodNotification(std::string method) {
  return WaitForEvent(base::BindRepeating(
      [](std::string method, const Event& event) -> bool {
        if (event.type != Event::Type::kNotification)
          return false;

        const std::string* candidate_method =
            event.value.GetDict().FindString("method");
        return (candidate_method && *candidate_method == method);
      },
      method));
}

void TestDevToolsAgentClient::DispatchProtocolResponse(
    blink::mojom::DevToolsMessagePtr message,
    int32_t call_id,
    blink::mojom::DevToolsSessionStatePtr updates) {
  LogEvent(Event::Type::kResponse, call_id, std::move(message));
}

void TestDevToolsAgentClient::DispatchProtocolNotification(
    blink::mojom::DevToolsMessagePtr message,
    blink::mojom::DevToolsSessionStatePtr updates) {
  LogEvent(Event::Type::kNotification, -1, std::move(message));
}

void TestDevToolsAgentClient::LogEvent(
    Event::Type type,
    int call_id,
    blink::mojom::DevToolsMessagePtr message) {
  crdtp::span<uint8_t> payload = ToSpan(message->data);
  EXPECT_EQ(use_binary_protocol_, crdtp::cbor::IsCBORMessage(payload));

  std::string payload_json;
  if (use_binary_protocol_) {
    // CBOR -> JSON.
    std::vector<uint8_t> json;
    crdtp::Status status = crdtp::json::ConvertCBORToJSON(payload, &json);
    CHECK(status.ok()) << status.Message();
    payload_json = ToString(json);
  } else {
    payload_json = ToString(payload);
  }

  // Now make it into a base::Value, to make it easy to look stuff up in it,
  // and queue it.
  std::optional<base::Value> val = base::JSONReader::Read(payload_json);
  CHECK(val.has_value());
  Event event;
  event.type = type;
  event.call_id = call_id;
  event.value = std::move(val.value());

  // Make sure it has proper session ID.
  const std::string* session = event.value.GetDict().FindString("sessionId");
  ASSERT_TRUE(session);
  EXPECT_EQ(session_id_, *session);

  events_.push_back(std::move(event));
}

std::string MakeInstrumentationBreakpointCommand(int seq_number,
                                                 const char* verb,
                                                 const char* event_name) {
  const char kTemplate[] = R"({
        "id":%d,
        "method":"EventBreakpoints.%sInstrumentationBreakpoint",
        "params": {
          "eventName": "%s"
        }})";
  return base::StringPrintf(kTemplate, seq_number, verb, event_name);
}

}  // namespace auction_worklet

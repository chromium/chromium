// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_DEVTOOLS_DEBUG_TEST_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_DEVTOOLS_DEBUG_TEST_UTIL_H_

#include <list>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"

namespace auction_worklet {

// This is a collection of helpers for testing debugging via the mojo
// blink.mojom.DevToolsAgent interface.
class TestDevToolsAgentClient : public blink::mojom::DevToolsSessionHost {
 public:
  // There are two channels used by devtools mojo protocol to talk to the
  // target:
  // - Main is where the script actually runs (V8 thread for us). It's not
  //   available when the script is blocked.
  // - IO runs on a separate channel (mojo thread for us). It's always
  // available, but isn't ordered with respect to other mojo pipes.
  enum class Channel { kMain, kIO };

  // Representation of a message sent back to the DevToolsSessionHost by the
  // debug target.
  struct Event {
    enum class Type { kResponse, kNotification };
    Type type;
    int call_id = -1;   // used for responses only.
    base::Value value;  // message body proper.
  };

  using EventPredicate = base::RepeatingCallback<bool(const Event&)>;

  // `use_binary_protocol` determines which protocol to ask of the agent and to
  // talk to it; all methods here will always talk JSON.
  TestDevToolsAgentClient(
      mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent,
      std::string session_id,
      bool use_binary_protocol);
  TestDevToolsAgentClient(const TestDevToolsAgentClient&) = delete;
  TestDevToolsAgentClient& operator=(const TestDevToolsAgentClient&) = delete;
  ~TestDevToolsAgentClient() override;

  // Invokes a debugger command with sequence number `call_id`, invoking method
  // `method`, and message body `payload` (which should have the same id and
  // method included, and must be in JSON).
  //
  // `channel` determines
  void RunCommand(Channel channel,
                  int call_id,
                  std::string method,
                  std::string payload);

  // Does everything RunCommand does, and also waits for a corresponding
  // Response back from the debugger.
  Event RunCommandAndWaitForResult(Channel channel,
                                   int call_id,
                                   std::string method,
                                   std::string payload);

  // Waits until an event matching `predicate` has been output by the attached
  // devtools agent, and returns a copy of it, removing it and all events before
  // it from event list. Will spin nested event loops.
  Event WaitForEvent(const EventPredicate& predicate);

  // Waits for a notification with method field matching `method`. Convenience
  // method using WaitForEvent() internally.
  Event WaitForMethodNotification(std::string method);

  // DevToolsSessionHost implementation:
  void DispatchProtocolResponse(
      blink::mojom::DevToolsMessagePtr message,
      int32_t call_id,
      blink::mojom::DevToolsSessionStatePtr updates) override;
  void DispatchProtocolNotification(
      blink::mojom::DevToolsMessagePtr message,
      blink::mojom::DevToolsSessionStatePtr updates) override;

 private:
  void LogEvent(Event::Type type,
                int call_id,
                blink::mojom::DevToolsMessagePtr message);

  std::string session_id_;
  bool use_binary_protocol_;

  mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent_;
  mojo::AssociatedRemote<blink::mojom::DevToolsSession> session_;
  mojo::Remote<blink::mojom::DevToolsSession> io_session_;
  mojo::AssociatedReceiver<blink::mojom::DevToolsSessionHost> receiver_;

  std::list<Event> events_;
};

// Returns JSON for EventBreakpoints.`verb`IntrumentationBreakpoint command with
// id `seq_number` targeting name `event_name`.
std::string MakeInstrumentationBreakpointCommand(int seq_number,
                                                 const char* verb,
                                                 const char* event_name);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_DEVTOOLS_DEBUG_TEST_UTIL_H_

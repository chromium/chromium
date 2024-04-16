// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_V8_DEBUG_TEST_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_V8_DEBUG_TEST_UTIL_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "url/gurl.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

// This is a collection of helpers for testing debugging by directly setting a
// V8Inspector (with minimalist test versions of various dependencies) on an
// AuctionV8Helper.

class AuctionV8Helper;

// Absolutely minimal implelementation of V8InspectorClient, which isn't
// good enough for anything like breakpoints.
class TestInspectorClient : public v8_inspector::V8InspectorClient {
 public:
  explicit TestInspectorClient(AuctionV8Helper* helper);
  TestInspectorClient(const TestInspectorClient&) = delete;
  TestInspectorClient& operator=(const TestInspectorClient&) = delete;

  void runIfWaitingForDebugger(int context_group_id) override;

 private:
  raw_ptr<AuctionV8Helper> v8_helper_;
};

// A thread-safe output channel that records everything that V8 tells it,
// and lets one wait for particular events. It also keeps track of a
// v8_inspector::V8InspectorSession* pointer to send commands the other way.
class TestChannel : public v8_inspector::V8Inspector::Channel {
 public:
  // Representation of a message sent back by V8's debugger code.
  struct Event {
    enum class Type { Response, Notification };
    Type type;
    int call_id = -1;  // used for responses only.

    // Payload of the message.
    base::Value value;
  };

  using EventPredicate = base::RepeatingCallback<bool(const Event&)>;

  // TestChannel instances are created via
  // ScopedInspectorSupport::ConnectDebuggerSession, and are owned by
  // ScopedInspectorSupport.
  TestChannel(const TestChannel&) = delete;
  TestChannel& operator=(const TestChannel&) = delete;
  ~TestChannel() override;

  void SetInspectorSession(
      v8_inspector::V8InspectorSession* v8_inspector_session) {
    v8_inspector_session_ = v8_inspector_session;
  }

  // Executes a debugger command with sequence number `call_id`, invoking method
  // `method`, and message body `payload` (which should have the same id and
  // method included, and must be in JSON), and waits for a corresponding
  // Response back from the debugger.
  //
  // Can be called on any thread.
  Event RunCommandAndWaitForResult(int call_id,
                                   std::string method,
                                   std::string payload);

  // Can be called on any thread, but the world state needs to be stable for it
  // to be possible to interpret the return value in a non-flaky way.
  std::list<Event> TakeAllEvents();

  // Waits until an event matching `predicate` has been output by v8, and
  // returns it, removing it from event list. All events previously will also be
  // considered as no longer eligible for WaitForEvent() (but will show up in
  // TakeAllEvents). Can be called on any thread.
  Event WaitForEvent(EventPredicate predicate);

  // Waits for a notification with method field matching `method`.
  Event WaitForMethodNotification(const std::string& method);

  // Waits for the next notification with the "Runtime.consoleAPICalled" method,
  // expecting it to have the provided details. `json_args` is the JSON
  // representation of the expected arguments.
  void WaitForAndValidateConsoleMessage(std::string_view type,
                                        std::string_view json_args,
                                        size_t stack_trace_size,
                                        std::string_view function,
                                        const GURL& url,
                                        int line_number);

  // Checks that there are no more console events, after spinning the run loop
  // until idle.
  void ExpectNoMoreConsoleEvents();

  // v8_inspector::V8Inspector::Channel implementation.
  void sendResponse(
      int call_id,
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void sendNotification(
      std::unique_ptr<v8_inspector::StringBuffer> message) override;
  void flushProtocolNotifications() override;

 private:
  friend class ScopedInspectorSupport;
  explicit TestChannel(AuctionV8Helper* v8_helper);

  void LogEvent(Event::Type type,
                int call_id,
                std::unique_ptr<v8_inspector::StringBuffer> message);

  // Can be called on any thread.
  void RunCommand(int call_id, std::string method, std::string payload);

  void RunCommandOnV8Thread(int call_id,
                            std::string method,
                            std::string payload);

  scoped_refptr<AuctionV8Helper> v8_helper_;
  raw_ptr<v8_inspector::V8InspectorSession> v8_inspector_session_;
  base::Lock lock_;
  base::ConditionVariable wake_up_ GUARDED_BY(lock_);
  std::list<Event> seen_events_ GUARDED_BY(lock_);
  std::list<Event> events_ GUARDED_BY(lock_);
};

// Class that helps set a V8Inspector w/a TestInspectorClient on an
// AuctionV8Helper, and clean it up properly. Assumes v8 thread is separate from
// main thread it runs on. Also helps with connecting debugger sessions.
class ScopedInspectorSupport {
 public:
  explicit ScopedInspectorSupport(AuctionV8Helper* v8_helper);
  ScopedInspectorSupport(const ScopedInspectorSupport&) = delete;
  ~ScopedInspectorSupport();
  ScopedInspectorSupport& operator=(const ScopedInspectorSupport&) = delete;

  // Connects a debugger session for given `context_group_id`. Will spin an
  // event loop.  Returned object is owned by this.
  TestChannel* ConnectDebuggerSession(int context_group_id);

  // Wraps ConnectDebuggerSession(), but also calls RunCommandAndWaitForResult()
  // with "debugger.enable", to enable debugging.
  TestChannel* ConnectDebuggerSessionAndRuntimeEnable(int context_group_id);

 private:
  struct V8State {
    V8State();
    ~V8State();
    scoped_refptr<AuctionV8Helper> v8_helper_;
    std::unique_ptr<TestInspectorClient> inspector_client_;
    std::vector<std::unique_ptr<TestChannel>> output_channels_;
    std::vector<std::unique_ptr<v8_inspector::V8InspectorSession>>
        inspector_sessions_;
  };

  void ConnectDebuggerSessionOnV8Thread(int context_group_id,
                                        TestChannel** result,
                                        base::OnceClosure done);

  // `v8_state_` is created on main thread, and used and destroyed on V8 thread,
  // except it's safe to access `v8_state_->v8_helper_->v8_runner()` from main
  // thread as well.
  std::unique_ptr<V8State, base::OnTaskRunnerDeleter> v8_state_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_V8_DEBUG_TEST_UTIL_H_

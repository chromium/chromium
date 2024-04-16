// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/values_test_util.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_v8_inspector_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "v8/include/v8-inspector.h"

namespace auction_worklet {

namespace {

std::string ToString(v8_inspector::StringView sv) {
  std::vector<uint8_t> string_bytes = GetStringBytes(sv);
  return std::string(reinterpret_cast<const char*>(string_bytes.data()),
                     string_bytes.size());
}

}  // namespace

TestInspectorClient::TestInspectorClient(AuctionV8Helper* helper)
    : v8_helper_(helper) {}

void TestInspectorClient::runIfWaitingForDebugger(int context_group_id) {
  v8_helper_->Resume(context_group_id);
}

TestChannel::TestChannel(AuctionV8Helper* v8_helper)
    : v8_helper_(v8_helper), wake_up_(&lock_) {}

TestChannel::~TestChannel() = default;

TestChannel::Event TestChannel::RunCommandAndWaitForResult(
    int call_id,
    std::string method,
    std::string payload) {
  RunCommand(call_id, std::move(method), std::move(payload));
  // Find a matching Response --- with same sequence number both externally and
  // inside the message's id field.
  return WaitForEvent(base::BindRepeating(
      [](int call_id, const Event& event) {
        return event.type == Event::Type::Response &&
               event.call_id == call_id && event.value.is_dict() &&
               event.value.GetDict().FindInt("id") == call_id;
      },
      call_id));
}

std::list<TestChannel::Event> TestChannel::TakeAllEvents() {
  base::AutoLock hold_lock(lock_);
  seen_events_.splice(seen_events_.end(), events_);
  return std::move(seen_events_);
}

TestChannel::Event TestChannel::WaitForEvent(EventPredicate predicate) {
  base::AutoLock hold_lock(lock_);
  while (true) {
    while (!events_.empty()) {
      Event event = std::move(*events_.begin());
      events_.pop_front();
      if (predicate.Run(event))
        return event;
      seen_events_.push_back(std::move(event));
    }
    wake_up_.Wait();
  }
}

TestChannel::Event TestChannel::WaitForMethodNotification(
    const std::string& method) {
  return WaitForEvent(base::BindRepeating(
      [](const std::string& method, const TestChannel::Event& event) -> bool {
        if (event.type != TestChannel::Event::Type::Notification)
          return false;

        const std::string* candidate_method =
            event.value.GetDict().FindString("method");
        return (candidate_method && *candidate_method == method);
      },
      method));
}

void TestChannel::WaitForAndValidateConsoleMessage(std::string_view type,
                                                   std::string_view json_args,
                                                   size_t stack_trace_size,
                                                   std::string_view function,
                                                   const GURL& url,
                                                   int line_number) {
  TestChannel::Event message =
      WaitForMethodNotification("Runtime.consoleAPICalled");
  const std::string* actual_type =
      message.value.GetDict().FindStringByDottedPath("params.type");
  ASSERT_TRUE(actual_type);
  EXPECT_EQ(type, *actual_type);
  const base::Value::List* args =
      message.value.GetDict().FindListByDottedPath("params.args");
  ASSERT_TRUE(args);
  EXPECT_THAT(*args, base::test::IsJson(json_args));

  const base::Value::List* stack_trace =
      message.value.GetDict().FindListByDottedPath(
          "params.stackTrace.callFrames");
  ASSERT_TRUE(stack_trace);
  ASSERT_EQ(stack_trace_size, stack_trace->size());
  const base::Value::Dict* stack_trace_dict = (*stack_trace)[0].GetIfDict();
  ASSERT_TRUE(stack_trace_dict);
  EXPECT_EQ(function, *stack_trace_dict->FindString("functionName"));
  EXPECT_EQ(url.spec(), *stack_trace_dict->FindString("url"));
  EXPECT_EQ(line_number, stack_trace_dict->FindInt("lineNumber"));
}

void TestChannel::ExpectNoMoreConsoleEvents() {
  base::RunLoop().RunUntilIdle();
  base::AutoLock hold_lock(lock_);
  for (const auto& event : events_) {
    if (event.type == TestChannel::Event::Type::Notification) {
      EXPECT_NE(*event.value.GetDict().FindString("method"),
                "Runtime.consoleAPICalled");
    }
  }
}

void TestChannel::sendResponse(
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK(v8_helper_->v8_runner()->RunsTasksInCurrentSequence());
  LogEvent(Event::Type::Response, call_id, std::move(message));
}

void TestChannel::sendNotification(
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK(v8_helper_->v8_runner()->RunsTasksInCurrentSequence());
  LogEvent(Event::Type::Notification, /*call_id=*/-1, std::move(message));
}

void TestChannel::flushProtocolNotifications() {
  DCHECK(v8_helper_->v8_runner()->RunsTasksInCurrentSequence());
}

void TestChannel::LogEvent(
    Event::Type type,
    int call_id,
    std::unique_ptr<v8_inspector::StringBuffer> message) {
  DCHECK(v8_helper_->v8_runner()->RunsTasksInCurrentSequence());

  // For TestChannel we always talk JSON.  Make it into a base::Value, to make
  // it easy to look stuff up in it.
  std::string message_str = ToString(message->string());
  std::optional<base::Value> val = base::JSONReader::Read(message_str);
  CHECK(val.has_value()) << message_str;
  Event event;
  event.type = type;
  event.call_id = call_id;
  event.value = std::move(val.value());

  {
    base::AutoLock hold_lock(lock_);
    events_.push_back(std::move(event));
    wake_up_.Signal();
  }
}

void TestChannel::RunCommand(int call_id,
                             std::string method,
                             std::string payload) {
  // The Unretained is safe since this is posted to the same runner as
  // ScopedInspectorSupport::V8State destruction, which is what destroys this.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestChannel::RunCommandOnV8Thread, base::Unretained(this),
                     call_id, std::move(method), std::move(payload)));
}

void TestChannel::RunCommandOnV8Thread(int call_id,
                                       std::string method,
                                       std::string payload) {
  DCHECK(v8_helper_->v8_runner()->RunsTasksInCurrentSequence());
  CHECK(
      v8_inspector::V8InspectorSession::canDispatchMethod(ToStringView(method)))
      << method << " " << payload;
  // Need isolate access.
  AuctionV8Helper::FullIsolateScope v8_scope(v8_helper_.get());

  // Send over the JSON message; we don't deal with CBOR in this fixture.
  v8_inspector_session_->dispatchProtocolMessage(ToStringView(payload));
}

ScopedInspectorSupport::ScopedInspectorSupport(AuctionV8Helper* v8_helper)
    : v8_state_(new V8State,
                base::OnTaskRunnerDeleter(v8_helper->v8_runner())) {
  DCHECK(!v8_helper->v8_runner()->RunsTasksInCurrentSequence());
  v8_state_->v8_helper_ = v8_helper;
  v8_state_->inspector_client_ =
      std::make_unique<TestInspectorClient>(v8_helper);
  v8_helper->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<AuctionV8Helper> v8_helper,
             TestInspectorClient* client) {
            v8_helper->SetV8InspectorForTesting(
                v8_inspector::V8Inspector::create(v8_helper->isolate(),
                                                  client));
          },
          v8_state_->v8_helper_, v8_state_->inspector_client_.get()));
}

ScopedInspectorSupport::~ScopedInspectorSupport() = default;

TestChannel* ScopedInspectorSupport::ConnectDebuggerSession(
    int context_group_id) {
  TestChannel* result = nullptr;
  base::RunLoop run_loop;
  v8_state_->v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ScopedInspectorSupport::ConnectDebuggerSessionOnV8Thread,
                     base::Unretained(this), context_group_id, &result,
                     run_loop.QuitClosure()));
  run_loop.Run();
  return result;
}

TestChannel* ScopedInspectorSupport::ConnectDebuggerSessionAndRuntimeEnable(
    int context_group_id) {
  TestChannel* channel = ConnectDebuggerSession(context_group_id);
  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  return channel;
}

ScopedInspectorSupport::V8State::V8State() = default;
ScopedInspectorSupport::V8State::~V8State() {
  output_channels_.clear();
  inspector_sessions_.clear();

  // Delete inspector after `inspector_sessions_`, before `inspector_client`_
  v8_helper_->SetV8InspectorForTesting(
      std::unique_ptr<v8_inspector::V8Inspector>());

  inspector_client_.reset();
}

void ScopedInspectorSupport::ConnectDebuggerSessionOnV8Thread(
    int context_group_id,
    TestChannel** result,
    base::OnceClosure done) {
  DCHECK(v8_state_->v8_helper_->v8_runner()->RunsTasksInCurrentSequence());

  auto test_channel =
      base::WrapUnique(new TestChannel(v8_state_->v8_helper_.get()));
  *result = test_channel.get();

  auto session = v8_state_->v8_helper_->inspector()->connect(
      context_group_id, test_channel.get(), v8_inspector::StringView(),
      v8_inspector::V8Inspector::kFullyTrusted);
  test_channel->SetInspectorSession(session.get());
  v8_state_->output_channels_.push_back(std::move(test_channel));
  v8_state_->inspector_sessions_.push_back(std::move(session));

  std::move(done).Run();
}

}  // namespace auction_worklet

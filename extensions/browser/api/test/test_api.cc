// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/test/test_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/test/test_api_observer_registry.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/test.h"

namespace {

// If you see this error in your test, you need to set the config state
// to be returned by chrome.test.getConfig().  Do this by calling
// TestGetConfigFunction::set_test_config_state(Value* state)
// in test set up.
const char kNoTestConfigDataError[] = "Test configuration was not set.";

const char kNotTestProcessError[] =
    "The chrome.test namespace is only available in tests.";

// Dispatches or queues a test notification (`chrome.test.onTestStarted` or
// `chrome.test.onTestFinished`). When no event listener is currently active in
// `EventRouter` outside `source_process_id`, the event is retained in a
// queue. Otherwise, the event is broadcast immediately across all processes.
void BroadcastOrQueueTestNotification(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    base::ListValue event_details,
    content::ChildProcessId source_process_id) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(browser_context);
  if (!event_router) {  // Unit tests may not have an event router.
    return;
  }

  CHECK(!source_process_id.is_null());

  if (!event_router->HasEventListenerOutsideProcess(event_name,
                                                    source_process_id)) {
    extensions::TestStartedAndFinishedEventQueue::Get(browser_context)
        ->EnqueueEvent(event_name, std::move(event_details), source_process_id);
    return;
  }

  auto event = std::make_unique<extensions::Event>(
      extensions::events::FOR_TEST, event_name, std::move(event_details),
      browser_context);
  event->exclude_process_id = source_process_id;
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace

namespace extensions {

void BroadcastOrQueueTestNotificationForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    base::ListValue event_details,
    content::ChildProcessId source_process_id) {
  BroadcastOrQueueTestNotification(browser_context, event_name,
                                   std::move(event_details), source_process_id);
}

namespace Log = api::test::Log;
namespace NotifyFail = api::test::NotifyFail;
namespace PassMessage = api::test::PassMessage;
namespace WaitForRoundTrip = api::test::WaitForRoundTrip;

TestExtensionFunction::~TestExtensionFunction() = default;

bool TestExtensionFunction::PreRunValidation(std::string* error) {
  if (!ExtensionFunction::PreRunValidation(error))
    return false;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    *error = kNotTestProcessError;
    return false;
  }
  return true;
}

TestNotifyPassFunction::~TestNotifyPassFunction() = default;

ExtensionFunction::ResponseAction TestNotifyPassFunction::Run() {
  TestApiObserverRegistry::GetInstance()->NotifyTestPassed(browser_context());
  return RespondNow(NoArguments());
}

TestNotifyFailFunction::~TestNotifyFailFunction() = default;

ExtensionFunction::ResponseAction TestNotifyFailFunction::Run() {
  std::optional<NotifyFail::Params> params = NotifyFail::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  TestApiObserverRegistry::GetInstance()->NotifyTestFailed(
      browser_context(), params->message);
  return RespondNow(NoArguments());
}

TestLogFunction::~TestLogFunction() = default;

ExtensionFunction::ResponseAction TestLogFunction::Run() {
  std::optional<Log::Params> params = Log::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  VLOG(1) << params->message;
  return RespondNow(NoArguments());
}

TestOpenFileUrlFunction::~TestOpenFileUrlFunction() = default;

ExtensionFunction::ResponseAction TestOpenFileUrlFunction::Run() {
  std::optional<api::test::OpenFileUrl::Params> params =
      api::test::OpenFileUrl::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  GURL file_url(params->url);
  EXTENSION_FUNCTION_VALIDATE(file_url.is_valid());
  EXTENSION_FUNCTION_VALIDATE(file_url.SchemeIsFile());

  ExtensionsAPIClient::Get()->OpenFileUrlForTesting(file_url,
                                                    browser_context());
  return RespondNow(NoArguments());
}

TestSendMessageFunction::TestSendMessageFunction() = default;

ExtensionFunction::ResponseAction TestSendMessageFunction::Run() {
  std::optional<PassMessage::Params> params =
      PassMessage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  bool listener_will_respond =
      TestApiObserverRegistry::GetInstance()->NotifyTestMessage(
          this, params->message);
  // If none of the listeners intend to respond, or one has already responded,
  // finish the function. We always reply to the message, even if it's just an
  // empty string.
  if (!listener_will_respond || response_) {
    if (!response_) {
      response_.emplace(WithArguments(std::string()));
    }
    return RespondNow(std::move(*response_));
  }
  // Otherwise, wait for a reply.
  waiting_ = true;
  return RespondLater();
}

TestSendMessageFunction::~TestSendMessageFunction() = default;

void TestSendMessageFunction::Reply(const std::string& message) {
  DCHECK(!response_);
  response_.emplace(WithArguments(message));
  if (waiting_)
    Respond(std::move(*response_));
}

void TestSendMessageFunction::ReplyWithError(const std::string& error) {
  DCHECK(!response_);
  response_.emplace(Error(error));
  if (waiting_)
    Respond(std::move(*response_));
}

TestSendScriptResultFunction::TestSendScriptResultFunction() = default;
TestSendScriptResultFunction::~TestSendScriptResultFunction() = default;

ExtensionFunction::ResponseAction TestSendScriptResultFunction::Run() {
  std::optional<api::test::SendScriptResult::Params> params =
      api::test::SendScriptResult::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  TestApiObserverRegistry::GetInstance()->NotifyScriptResult(params->result);
  return RespondNow(NoArguments());
}

// static
base::LazyInstance<BrowserContextKeyedAPIFactory<
    TestStartedAndFinishedEventQueue>>::DestructorAtExit
    g_test_started_and_finished_event_queue_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<TestStartedAndFinishedEventQueue>*
TestStartedAndFinishedEventQueue::GetFactoryInstance() {
  return g_test_started_and_finished_event_queue_factory.Pointer();
}

// static
TestStartedAndFinishedEventQueue* TestStartedAndFinishedEventQueue::Get(
    content::BrowserContext* browser_context) {
  return BrowserContextKeyedAPIFactory<TestStartedAndFinishedEventQueue>::Get(
      browser_context);
}

template <>
void BrowserContextKeyedAPIFactory<
    TestStartedAndFinishedEventQueue>::DeclareFactoryDependencies() {
  DependsOn(EventRouterFactory::GetInstance());
}

TestStartedAndFinishedEventQueue::QueuedEvent::QueuedEvent(
    base::ListValue args,
    content::ChildProcessId source_process_id)
    : event_args(std::move(args)), source_process_id(source_process_id) {}

TestStartedAndFinishedEventQueue::QueuedEvent::QueuedEvent(QueuedEvent&&) =
    default;

TestStartedAndFinishedEventQueue::QueuedEvent&
TestStartedAndFinishedEventQueue::QueuedEvent::operator=(QueuedEvent&&) =
    default;

TestStartedAndFinishedEventQueue::QueuedEvent::~QueuedEvent() = default;

// Initializes the event queue and registers this object as an observer
// for test events (`chrome.test.onTestStarted` and
// `chrome.test.onTestFinished`).
TestStartedAndFinishedEventQueue::TestStartedAndFinishedEventQueue(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {  // Unit tests may not have an `EventRouter`.
    event_router->RegisterObserver(this, api::test::OnTestStarted::kEventName);
    event_router->RegisterObserver(this, api::test::OnTestFinished::kEventName);
  }
}

TestStartedAndFinishedEventQueue::~TestStartedAndFinishedEventQueue() {
  if (browser_context_) {
    EventRouter* event_router = EventRouter::Get(browser_context_);
    if (event_router) {  // Unit tests may not have an `EventRouter`.
      event_router->UnregisterObserver(this);
    }
  }
}

// Unregisters this object as an observer from `EventRouter` when the
// `KeyedService` is shutting down.
void TestStartedAndFinishedEventQueue::Shutdown() {
  if (browser_context_) {
    EventRouter* event_router = EventRouter::Get(browser_context_);
    if (event_router) {  // Unit tests may not have an `EventRouter`.
      event_router->UnregisterObserver(this);
    }
    browser_context_ = nullptr;
  }
}

// Appends a new event containing the event args to the pending
// queue for `event_name`.
void TestStartedAndFinishedEventQueue::EnqueueEvent(
    const std::string& event_name,
    base::ListValue event_args,
    content::ChildProcessId source_process_id) {
  pending_events_[event_name].emplace_back(std::move(event_args),
                                           source_process_id);
}

// Called when an event listener registers with `EventRouter`. If pending
// events exist for `details.event_name`, broadcasts each queued event
// unless the event originated from the same process.
void TestStartedAndFinishedEventQueue::OnListenerAdded(
    const EventListenerInfo& details) {
  auto pending_events_it = pending_events_.find(details.event_name);

  // No events to dispatch.
  if (pending_events_it == pending_events_.end()) {
    return;
  }

  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {  // Unit tests may not have an `EventRouter`.
    return;
  }

  // Stores queued events that cannot be dispatched to the newly added listener
  // because they originated from the same process. These events remain in the
  // queue until a listener in a different process registers.
  std::vector<QueuedEvent> retained_events;
  size_t num_pending_events_before_dispatch = pending_events_it->second.size();

  // Loop through each queued event and broadcast it if the listener
  // appears valid.
  for (auto& queued_event : pending_events_it->second) {
    // Don't dispatch the pending event back to its original process.
    if (!details.render_process_id.is_null() &&
        !queued_event.source_process_id.is_null() &&
        details.render_process_id == queued_event.source_process_id) {
      retained_events.push_back(std::move(queued_event));
      continue;
    }

    auto event = std::make_unique<Event>(events::FOR_TEST, details.event_name,
                                         std::move(queued_event.event_args),
                                         browser_context_);
    event->exclude_process_id = queued_event.source_process_id;
    // Rather than dispatch to just this listener, we broadcast it to simplify
    // the logic here and in case there are other listeners registered in other
    // processes.
    event_router->BroadcastEvent(std::move(event));
  }

  // Confirm that no new events were synchronously added to `pending_events_`
  // during event dispatch. Otherwise, they would be overwritten below.
  CHECK(pending_events_it->second.size() == num_pending_events_before_dispatch);

  // Clear the pending events entry since any that we're keeping are in
  // `retained_events`.
  pending_events_.erase(pending_events_it);

  if (retained_events.empty()) {
    return;
  }

  // Add the events we couldn't dispatch back to `pending_events_`.
  pending_events_[details.event_name] = std::move(retained_events);
}

// Returns the number of pending events currently retained in the queue
// for `event_name`.
size_t TestStartedAndFinishedEventQueue::GetPendingEventsCountForTesting(
    const std::string& event_name) const {
  auto it = pending_events_.find(event_name);
  return it != pending_events_.end() ? it->second.size() : 0;
}

// Returns `true` if there are currently zero pending events retained
// across all event names.
bool TestStartedAndFinishedEventQueue::IsQueueEmptyForTesting() const {
  return pending_events_.empty();
}

TestNotifyTestStartedFunction::~TestNotifyTestStartedFunction() = default;

ExtensionFunction::ResponseAction TestNotifyTestStartedFunction::Run() {
  std::optional<api::test::NotifyTestStarted::Params> params =
      api::test::NotifyTestStarted::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::ListValue event_details;
  base::DictValue info;
  info.Set("testName", params->test_name);
  event_details.Append(std::move(info));

  BroadcastOrQueueTestNotification(
      browser_context(), api::test::OnTestStarted::kEventName,
      std::move(event_details),
      content::ChildProcessId::FromUnsafeValue(source_process_id()));

  return RespondNow(NoArguments());
}

TestNotifyTestFinishedFunction::~TestNotifyTestFinishedFunction() = default;

ExtensionFunction::ResponseAction TestNotifyTestFinishedFunction::Run() {
  std::optional<api::test::NotifyTestFinished::Params> params =
      api::test::NotifyTestFinished::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::ListValue event_details;
  base::DictValue info;
  info.Set("testName", params->test_name);
  info.Set("result", params->result);
  info.Set("remainingTests", params->remaining_tests);
  info.Set("assertionDescription", params->assertion_description);
  if (params->message) {
    info.Set("message", *params->message);
  }
  event_details.Append(std::move(info));

  BroadcastOrQueueTestNotification(
      browser_context(), api::test::OnTestFinished::kEventName,
      std::move(event_details),
      content::ChildProcessId::FromUnsafeValue(source_process_id()));

  return RespondNow(NoArguments());
}

// static
void TestGetConfigFunction::set_test_config_state(base::DictValue* value) {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  test_config_state->set_config_state(value);
}

TestGetConfigFunction::TestConfigState::TestConfigState()
    : config_state_(nullptr) {}

// static
TestGetConfigFunction::TestConfigState*
TestGetConfigFunction::TestConfigState::GetInstance() {
  return base::Singleton<TestConfigState>::get();
}

TestGetConfigFunction::~TestGetConfigFunction() = default;

ExtensionFunction::ResponseAction TestGetConfigFunction::Run() {
  TestConfigState* test_config_state = TestConfigState::GetInstance();
  if (!test_config_state->config_state())
    return RespondNow(Error(kNoTestConfigDataError));
  return RespondNow(WithArguments(test_config_state->config_state()->Clone()));
}

TestWaitForRoundTripFunction::~TestWaitForRoundTripFunction() = default;

ExtensionFunction::ResponseAction TestWaitForRoundTripFunction::Run() {
  std::optional<WaitForRoundTrip::Params> params =
      WaitForRoundTrip::Params::Create(args());
  return RespondNow(WithArguments(params->message));
}

}  // namespace extensions

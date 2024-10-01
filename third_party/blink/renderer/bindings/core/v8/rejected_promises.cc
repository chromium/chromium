// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/rejected_promises.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_promise_rejection_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/events/promise_rejection_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

static const unsigned kMaxReportedHandlersPendingResolution = 1000;

class RejectedPromises::Message final {
 public:
  Message(ScriptState* script_state,
          v8::Local<v8::Promise> promise,
          v8::Local<v8::Value> exception,
          const String& error_message,
          std::unique_ptr<SourceLocation> location,
          SanitizeScriptErrors sanitize_script_errors)
      : script_state_(script_state),
        promise_(script_state->GetIsolate(), promise),
        exception_(script_state->GetIsolate(), exception),
        error_message_(error_message),
        location_(std::move(location)),
        promise_rejection_id_(0),
        collected_(false),
        should_log_to_console_(true),
        sanitize_script_errors_(sanitize_script_errors) {}

  bool IsCollected() { return collected_ || !script_state_->ContextIsValid(); }

  bool HasPromise(v8::Local<v8::Value> promise) { return promise_ == promise; }

  void Report() {
    if (!script_state_->ContextIsValid())
      return;
    // If execution termination has been triggered, quietly bail out.
    if (script_state_->GetIsolate()->IsExecutionTerminating())
      return;
    ExecutionContext* execution_context = ExecutionContext::From(script_state_);
    if (!execution_context)
      return;

    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Promise> promise =
        promise_.NewLocal(script_state_->GetIsolate());
    v8::Local<v8::Value> reason =
        exception_.NewLocal(script_state_->GetIsolate());
    if (promise.IsEmpty()) {
      return;
    }
    DCHECK(!HasHandler());

    EventTarget* target = execution_context->ErrorEventTarget();
    if (target &&
        sanitize_script_errors_ == SanitizeScriptErrors::kDoNotSanitize) {
      PromiseRejectionEventInit* init = PromiseRejectionEventInit::Create();
      init->setPromise(ScriptPromise<IDLAny>::FromV8Promise(
          script_state_->GetIsolate(), promise));
      init->setReason(ScriptValue(script_state_->GetIsolate(), reason));
      init->setCancelable(true);
      PromiseRejectionEvent* event = PromiseRejectionEvent::Create(
          script_state_, event_type_names::kUnhandledrejection, init);
      // Log to console if event was not canceled.
      should_log_to_console_ =
          target->DispatchEvent(*event) == DispatchEventResult::kNotCanceled;
    }

    if (should_log_to_console_) {
      ThreadDebugger* debugger =
          ThreadDebugger::From(script_state_->GetIsolate());
      if (debugger) {
        promise_rejection_id_ = debugger->PromiseRejected(
            script_state_->GetContext(), error_message_, reason,
            std::move(location_));
      }
    }

    location_.reset();
  }

  void Revoke() {
    if (!script_state_->ContextIsValid()) {
      // If the context is not valid, the frame is removed for example, then do
      // nothing.
      return;
    }
    ExecutionContext* execution_context = ExecutionContext::From(script_state_);
    if (!execution_context)
      return;

    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Promise> promise =
        promise_.NewLocal(script_state_->GetIsolate());
    v8::Local<v8::Value> reason =
        exception_.NewLocal(script_state_->GetIsolate());
    if (promise.IsEmpty()) {
      return;
    }

    EventTarget* target = execution_context->ErrorEventTarget();
    if (target &&
        sanitize_script_errors_ == SanitizeScriptErrors::kDoNotSanitize) {
      PromiseRejectionEventInit* init = PromiseRejectionEventInit::Create();
      init->setPromise(ScriptPromise<IDLAny>::FromV8Promise(
          script_state_->GetIsolate(), promise));
      init->setReason(ScriptValue(script_state_->GetIsolate(), reason));
      PromiseRejectionEvent* event = PromiseRejectionEvent::Create(
          script_state_, event_type_names::kRejectionhandled, init);
      target->DispatchEvent(*event);
    }

    if (should_log_to_console_ && promise_rejection_id_) {
      ThreadDebugger* debugger =
          ThreadDebugger::From(script_state_->GetIsolate());
      if (debugger) {
        debugger->PromiseRejectionRevoked(script_state_->GetContext(),
                                          promise_rejection_id_);
      }
    }
  }

  void MakePromiseWeak() {
    CHECK(!promise_.IsEmpty());
    CHECK(!promise_.IsWeak());
    promise_.SetWeak(this, &Message::DidCollectPromise);
    exception_.SetWeak(this, &Message::DidCollectException);
  }

  void MakePromiseStrong() {
    CHECK(!promise_.IsEmpty());
    CHECK(promise_.IsWeak());
    promise_.ClearWeak();
    exception_.ClearWeak();
  }

  bool HasHandler() {
    DCHECK(!IsCollected());
    ScriptState::Scope scope(script_state_);
    v8::Local<v8::Value> value = promise_.NewLocal(script_state_->GetIsolate());
    return v8::Local<v8::Promise>::Cast(value)->HasHandler();
  }

  ExecutionContext* GetContext() {
    return ExecutionContext::From(script_state_);
  }

 private:
  static void DidCollectPromise(const v8::WeakCallbackInfo<Message>& data) {
    data.GetParameter()->collected_ = true;
    data.GetParameter()->promise_.Clear();
  }

  static void DidCollectException(const v8::WeakCallbackInfo<Message>& data) {
    data.GetParameter()->exception_.Clear();
  }

  Persistent<ScriptState> script_state_;
  ScopedPersistent<v8::Promise> promise_;
  ScopedPersistent<v8::Value> exception_;
  String error_message_;
  std::unique_ptr<SourceLocation> location_;
  unsigned promise_rejection_id_;
  bool collected_;
  bool should_log_to_console_;
  SanitizeScriptErrors sanitize_script_errors_;
};

RejectedPromises::RejectedPromises() = default;

RejectedPromises::~RejectedPromises() = default;

void RejectedPromises::RejectedWithNoHandler(
    ScriptState* script_state,
    v8::PromiseRejectMessage data,
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    SanitizeScriptErrors sanitize_script_errors) {
  queue_.push_back(std::make_unique<Message>(
      script_state, data.GetPromise(), data.GetValue(), error_message,
      std::move(location), sanitize_script_errors));
}

void RejectedPromises::HandlerAdded(v8::PromiseRejectMessage data) {
  // First look it up in the pending messages and fast return, it'll be covered
  // by processQueue().
  for (auto it = queue_.begin(); it != queue_.end(); ++it) {
    if (!(*it)->IsCollected() && (*it)->HasPromise(data.GetPromise())) {
      queue_.erase(it);
      return;
    }
  }

  // Then look it up in the reported errors.
  for (wtf_size_t i = 0; i < reported_as_errors_.size(); ++i) {
    std::unique_ptr<Message>& message = reported_as_errors_.at(i);
    if (!message->IsCollected() && message->HasPromise(data.GetPromise())) {
      message->MakePromiseStrong();
      // Since we move out of `message` below, we need to pull `context` out in
      // a separate statement.
      ExecutionContext* context = message->GetContext();
      context->GetTaskRunner(TaskType::kDOMManipulation)
          ->PostTask(FROM_HERE,
                     WTF::BindOnce(&RejectedPromises::RevokeNow,
                                   scoped_refptr<RejectedPromises>(this),
                                   std::move(message)));
      reported_as_errors_.EraseAt(i);
      return;
    }
  }
}

void RejectedPromises::Dispose() {
  if (queue_.empty())
    return;

  ProcessQueueNow(std::move(queue_));
  queue_.clear();
}

void RejectedPromises::ProcessQueue() {
  if (queue_.empty())
    return;

  HeapHashMap<Member<ExecutionContext>, MessageQueue> queues;
  for (auto& message : queue_) {
    auto result = queues.insert(message->GetContext(), MessageQueue());
    result.stored_value->value.push_back(std::move(message));
  }
  queue_.clear();

  for (auto& kv : queues) {
    kv.key->GetTaskRunner(blink::TaskType::kDOMManipulation)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&RejectedPromises::ProcessQueueNow,
                                 scoped_refptr<RejectedPromises>(this),
                                 std::move(kv.value)));
  }
}

void RejectedPromises::ProcessQueueNow(MessageQueue queue) {
  // Remove collected handlers.
  auto new_end = std::remove_if(
      reported_as_errors_.begin(), reported_as_errors_.end(),
      [](const auto& message) { return message->IsCollected(); });
  reported_as_errors_.Shrink(
      static_cast<wtf_size_t>(new_end - reported_as_errors_.begin()));

  for (auto& message : queue) {
    if (message->IsCollected())
      continue;
    if (!message->HasHandler()) {
      message->Report();
      message->MakePromiseWeak();
      reported_as_errors_.push_back(std::move(message));
      if (reported_as_errors_.size() > kMaxReportedHandlersPendingResolution) {
        reported_as_errors_.EraseAt(0,
                                    kMaxReportedHandlersPendingResolution / 10);
      }
    }
  }
}

void RejectedPromises::RevokeNow(std::unique_ptr<Message> message) {
  message->Revoke();
}

}  // namespace blink

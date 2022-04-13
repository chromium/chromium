// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/closewatcher/close_watcher.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_close_watcher_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

namespace {

class DestroyOnAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  explicit DestroyOnAbortAlgorithm(CloseWatcher* watcher) : watcher_(watcher) {}

  void Run() override { watcher_->destroy(); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(watcher_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<CloseWatcher> watcher_;
};

}  // namespace

CloseWatcher::WatcherStack::WatcherStack(LocalDOMWindow* window)
    : receiver_(this, window), window_(window) {}

void CloseWatcher::WatcherStack::Add(CloseWatcher* watcher) {
  if (watchers_.IsEmpty()) {
    auto& host = window_->GetFrame()->GetLocalFrameHostRemote();
    host.SetCloseListener(receiver_.BindNewPipeAndPassRemote(
        window_->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  watchers_.insert(watcher);
}

void CloseWatcher::WatcherStack::Remove(CloseWatcher* watcher) {
  watchers_.erase(watcher);
  if (watchers_.IsEmpty()) {
    receiver_.reset();
  }
}

void CloseWatcher::WatcherStack::Trace(Visitor* visitor) const {
  visitor->Trace(watchers_);
  visitor->Trace(receiver_);
  visitor->Trace(window_);
}

void CloseWatcher::WatcherStack::EscapeKeyHandler(KeyboardEvent* event) {
  if (!watchers_.IsEmpty() && !event->DefaultHandled() && event->isTrusted() &&
      event->keyCode() == VKEY_ESCAPE) {
    Signal();
  }
}

bool CloseWatcher::WatcherStack::CheckForCreation() {
  if (HasActiveWatcher() &&
      !LocalFrame::ConsumeTransientUserActivation(window_->GetFrame())) {
    return false;
  }

  ConsumeCloseWatcherCancelability();
  return true;
}

// static
CloseWatcher* CloseWatcher::Create(LocalDOMWindow* window,
                                   CloseWatcherOptions* options) {
  if (!window->GetFrame())
    return nullptr;
  WatcherStack* stack = window->closewatcher_stack();
  if (!stack->CheckForCreation())
    return nullptr;
  return CreateInternal(window, *stack, options);
}

// static
CloseWatcher* CloseWatcher::Create(ScriptState* script_state,
                                   CloseWatcherOptions* options,
                                   ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CloseWatchers cannot be created in detached Windows.");
    return nullptr;
  }

  WatcherStack& stack = *window->closewatcher_stack();

  if (!stack.CheckForCreation()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Creating more than one CloseWatcher at a time requires a user "
        "activation.");
    return nullptr;
  }

  return CreateInternal(window, stack, options);
}

// static
CloseWatcher* CloseWatcher::CreateInternal(LocalDOMWindow* window,
                                           WatcherStack& stack,
                                           CloseWatcherOptions* options) {
  CloseWatcher* watcher = MakeGarbageCollected<CloseWatcher>(window);

  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      watcher->state_ = State::kClosed;
      return watcher;
    }
    signal->AddAlgorithm(
        MakeGarbageCollected<DestroyOnAbortAlgorithm>(watcher));
  }

  stack.Add(watcher);
  return watcher;
}

CloseWatcher::CloseWatcher(LocalDOMWindow* window)
    : ExecutionContextClient(window) {}

void CloseWatcher::close() {
  if (IsClosed() || dispatching_cancel_ || !DomWindow())
    return;

  WatcherStack& stack = *DomWindow()->closewatcher_stack();

  if (stack.CanCloseWatcherFireCancel()) {
    stack.ConsumeCloseWatcherCancelability();
    Event& cancel_event = *Event::CreateCancelable(event_type_names::kCancel);
    {
      base::AutoReset<bool> scoped_committing(&dispatching_cancel_, true);
      DispatchEvent(cancel_event);
    }
    if (cancel_event.defaultPrevented())
      return;
  }

  // These might have changed because of the event firing.
  if (IsClosed())
    return;
  if (DomWindow())
    DomWindow()->closewatcher_stack()->Remove(this);

  state_ = State::kClosed;
  DispatchEvent(*Event::Create(event_type_names::kClose));
}
void CloseWatcher::destroy() {
  if (IsClosed())
    return;
  if (DomWindow())
    DomWindow()->closewatcher_stack()->Remove(this);
  state_ = State::kClosed;
}

const AtomicString& CloseWatcher::InterfaceName() const {
  return event_target_names::kCloseWatcher;
}

void CloseWatcher::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink

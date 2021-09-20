// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/closewatcher/close_watcher.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

const char CloseWatcher::WatcherStack::kSupplementName[] =
    "CloseWatcher::WatcherStack";

CloseWatcher::WatcherStack& CloseWatcher::WatcherStack::From(
    LocalDOMWindow& window) {
  auto* stack = Supplement<LocalDOMWindow>::From<WatcherStack>(window);
  if (!stack) {
    stack = MakeGarbageCollected<WatcherStack>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, stack);
  }
  return *stack;
}

CloseWatcher::WatcherStack::WatcherStack(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), receiver_(this, &window) {}

void CloseWatcher::WatcherStack::Add(CloseWatcher* watcher) {
  if (watchers_.IsEmpty()) {
    GetSupplementable()->addEventListener(event_type_names::kKeyup, this,
                                          /*use_capture=*/false);
    auto& host = GetSupplementable()->GetFrame()->GetLocalFrameHostRemote();
    host.SetCloseListener(receiver_.BindNewPipeAndPassRemote(
        GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  watchers_.insert(watcher);
}

void CloseWatcher::WatcherStack::Remove(CloseWatcher* watcher) {
  watchers_.erase(watcher);
  if (watchers_.IsEmpty()) {
    GetSupplementable()->removeEventListener(event_type_names::kKeyup, this,
                                             /*use_capture=*/false);
    receiver_.reset();
  }
}

void CloseWatcher::WatcherStack::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(watchers_);
  visitor->Trace(receiver_);
}

void CloseWatcher::WatcherStack::Invoke(ExecutionContext*, Event* e) {
  DCHECK(!watchers_.IsEmpty());
  KeyboardEvent* event = DynamicTo<KeyboardEvent>(e);
  if (event && event->isTrusted() && event->keyCode() == VKEY_ESCAPE)
    Signal();
}

CloseWatcher* CloseWatcher::Create(ScriptState* script_state,
                                   ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CloseWatchers cannot be created in detached Windows.");
    return nullptr;
  }

  WatcherStack& stack = WatcherStack::From(*window);
  if (stack.HasActiveWatcher() &&
      !LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Creating more than one CloseWatcher at a time requires a user "
        "activation.");
    return nullptr;
  }

  CloseWatcher* watcher = MakeGarbageCollected<CloseWatcher>(window);
  stack.Add(watcher);
  return watcher;
}

CloseWatcher::CloseWatcher(LocalDOMWindow* window)
    : ExecutionContextClient(window) {}

void CloseWatcher::signalClosed() {
  if (IsClosed() || dispatching_cancel_)
    return;

  // TODO(domenic): only dispatch cancel if there's been user activation.
  Event& cancel_event = *Event::CreateCancelable(event_type_names::kCancel);
  {
    base::AutoReset<bool> scoped_committing(&dispatching_cancel_, true);
    DispatchEvent(cancel_event);
  }
  if (cancel_event.defaultPrevented()) {
    state_ = State::kModal;
  }
  Close();
}

void CloseWatcher::Close() {
  if (IsClosed())
    return;
  if (DomWindow())
    WatcherStack::From(*DomWindow()).Remove(this);
  state_ = State::kClosed;
  DispatchEvent(*Event::Create(event_type_names::kClose));
}

void CloseWatcher::destroy() {
  if (IsClosed())
    return;
  if (DomWindow())
    WatcherStack::From(*DomWindow()).Remove(this);
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

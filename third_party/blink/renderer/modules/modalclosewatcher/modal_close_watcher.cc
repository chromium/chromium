// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/modalclosewatcher/modal_close_watcher.h"

#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"

namespace blink {

const char ModalCloseWatcher::WatcherStack::kSupplementName[] =
    "ModalCloseWatcher::WatcherStack";

ModalCloseWatcher::WatcherStack& ModalCloseWatcher::WatcherStack::From(
    LocalDOMWindow& window) {
  auto* stack = Supplement<LocalDOMWindow>::From<WatcherStack>(window);
  if (!stack) {
    stack = MakeGarbageCollected<WatcherStack>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, stack);
  }
  return *stack;
}

ModalCloseWatcher::WatcherStack::WatcherStack(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window), receiver_(this, &window) {}

void ModalCloseWatcher::WatcherStack::Add(ModalCloseWatcher* watcher) {
  if (watchers_.IsEmpty()) {
    GetSupplementable()->addEventListener(event_type_names::kKeyup, this);
    auto& host = GetSupplementable()->GetFrame()->GetLocalFrameHostRemote();
    host.SetModalCloseListener(receiver_.BindNewPipeAndPassRemote(
        GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  watchers_.insert(watcher);
}

void ModalCloseWatcher::WatcherStack::Remove(ModalCloseWatcher* watcher) {
  watchers_.erase(watcher);
  if (watchers_.IsEmpty()) {
    GetSupplementable()->removeEventListener(event_type_names::kKeyup, this);
    receiver_.reset();
  }
}

void ModalCloseWatcher::WatcherStack::Trace(Visitor* visitor) const {
  NativeEventListener::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(watchers_);
  visitor->Trace(receiver_);
}

void ModalCloseWatcher::WatcherStack::Invoke(ExecutionContext*, Event* e) {
  DCHECK(!watchers_.IsEmpty());
  KeyboardEvent* event = DynamicTo<KeyboardEvent>(e);
  if (event && event->isTrusted() && event->keyCode() == VKEY_ESCAPE)
    Signal();
}

ModalCloseWatcher* ModalCloseWatcher::Create(ScriptState* script_state,
                                             ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  WatcherStack& stack = WatcherStack::From(*window);
  if (stack.HasActiveWatcher() &&
      !LocalFrame::HasTransientUserActivation(window->GetFrame())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotAllowedError,
        "Creating more than one ModalCloseWatcher at a time requires a user "
        "activation.");
    return nullptr;
  }

  ModalCloseWatcher* watcher = MakeGarbageCollected<ModalCloseWatcher>(window);
  stack.Add(watcher);
  return watcher;
}

ModalCloseWatcher::ModalCloseWatcher(LocalDOMWindow* window)
    : ExecutionContextClient(window) {}

void ModalCloseWatcher::signalClosed() {
  if (IsClosed() || dispatching_beforeclose_)
    return;
  Event& before_close_event =
      *Event::CreateCancelable(event_type_names::kBeforeclose);
  {
    base::AutoReset<bool> scoped_committing(&dispatching_beforeclose_, true);
    DispatchEvent(before_close_event);
  }
  if (before_close_event.defaultPrevented()) {
    state_ = State::kModal;
    // TODO(japhet): Make an async dialog here.
  }
  Close();
}

void ModalCloseWatcher::Close() {
  if (IsClosed())
    return;
  WatcherStack::From(*DomWindow()).Remove(this);
  state_ = State::kClosed;
  DispatchEvent(*Event::Create(event_type_names::kClose));
}

void ModalCloseWatcher::destroy() {
  if (IsClosed())
    return;
  WatcherStack::From(*DomWindow()).Remove(this);
  state_ = State::kClosed;
}

const AtomicString& ModalCloseWatcher::InterfaceName() const {
  return event_target_names::kModalCloseWatcher;
}

void ModalCloseWatcher::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink

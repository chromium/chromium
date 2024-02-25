// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/closewatcher/close_watcher.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_close_watcher_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/event_type_names.h"
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
  if (watchers_.empty()) {
    auto& host = window_->GetFrame()->GetLocalFrameHostRemote();
    host.SetCloseListener(receiver_.BindNewPipeAndPassRemote(
        window_->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }
  watchers_.insert(watcher);
}

void CloseWatcher::WatcherStack::Remove(CloseWatcher* watcher) {
  watchers_.erase(watcher);
  if (watchers_.empty()) {
    receiver_.reset();
  }
}

void CloseWatcher::WatcherStack::Trace(Visitor* visitor) const {
  visitor->Trace(watchers_);
  visitor->Trace(receiver_);
  visitor->Trace(window_);
}

void CloseWatcher::WatcherStack::EscapeKeyHandler(KeyboardEvent* event) {
  if (!watchers_.empty() && !event->DefaultHandled() && event->isTrusted() &&
      event->keyCode() == VKEY_ESCAPE) {
    Signal();
  }
}

void CloseWatcher::WatcherStack::Signal() {
  while (!watchers_.empty()) {
    CloseWatcher* watcher = watchers_.back();
    watcher->requestClose();

    if (!watcher->IsGroupedWithPrevious()) {
      break;
    }
  }
}

bool CloseWatcher::WatcherStack::HasConsumedFreeWatcher() const {
  for (const auto& watcher : watchers_) {
    if (!watcher->created_with_user_activation_) {
      return true;
    }
  }
  return false;
}

// static
CloseWatcher* CloseWatcher::Create(LocalDOMWindow& window) {
  if (!window.GetFrame()) {
    return nullptr;
  }

  WatcherStack& stack = *window.closewatcher_stack();
  return CreateInternal(window, stack, nullptr);
}

// static
CloseWatcher* CloseWatcher::Create(ScriptState* script_state,
                                   CloseWatcherOptions* options,
                                   ExceptionState& exception_state) {
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window || !window->GetFrame()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "CloseWatchers cannot be created in detached Windows.");
    return nullptr;
  }

  WatcherStack& stack = *window->closewatcher_stack();
  return CreateInternal(*window, stack, options);
}

// static
CloseWatcher* CloseWatcher::CreateInternal(LocalDOMWindow& window,
                                           WatcherStack& stack,
                                           CloseWatcherOptions* options) {
  CHECK(window.document()->IsActive());

  CloseWatcher* watcher = MakeGarbageCollected<CloseWatcher>(window);

  if (window.GetFrame()->IsHistoryUserActivationActive()) {
    window.GetFrame()->ConsumeHistoryUserActivation();
    watcher->created_with_user_activation_ = true;
    watcher->grouped_with_previous_ = false;
  } else if (!stack.HasConsumedFreeWatcher()) {
    watcher->created_with_user_activation_ = false;
    watcher->grouped_with_previous_ = false;
  } else {
    watcher->created_with_user_activation_ = false;
    watcher->grouped_with_previous_ = true;
  }

  if (options && options->hasSignal()) {
    AbortSignal* signal = options->signal();
    if (signal->aborted()) {
      watcher->state_ = State::kClosed;
      return watcher;
    }
    watcher->abort_handle_ = signal->AddAlgorithm(
        MakeGarbageCollected<DestroyOnAbortAlgorithm>(watcher));
  }

  stack.Add(watcher);
  return watcher;
}

CloseWatcher::CloseWatcher(LocalDOMWindow& window)
    : ExecutionContextClient(&window) {}

void CloseWatcher::requestClose() {
  if (IsClosed() || dispatching_cancel_ || !DomWindow()) {
    return;
  }

  if (DomWindow()->GetFrame()->IsHistoryUserActivationActive()) {
    Event& cancel_event = *Event::CreateCancelable(event_type_names::kCancel);
    {
      base::AutoReset<bool> scoped_committing(&dispatching_cancel_, true);
      DispatchEvent(cancel_event);
    }
    if (cancel_event.defaultPrevented()) {
      if (DomWindow()) {
        DomWindow()->GetFrame()->ConsumeHistoryUserActivation();
      }
      return;
    }
  }

  close();
}

void CloseWatcher::close() {
  if (IsClosed()) {
    return;
  }
  if (!DomWindow() || !DomWindow()->document()->IsActive()) {
    return;
  }

  destroy();

  DispatchEvent(*Event::Create(event_type_names::kClose));
}

void CloseWatcher::destroy() {
  if (IsClosed()) {
    return;
  }
  if (DomWindow()) {
    DomWindow()->closewatcher_stack()->Remove(this);
  }
  state_ = State::kClosed;
  abort_handle_.Clear();
}

const AtomicString& CloseWatcher::InterfaceName() const {
  return event_target_names::kCloseWatcher;
}

void CloseWatcher::Trace(Visitor* visitor) const {
  visitor->Trace(abort_handle_);
  EventTarget::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink

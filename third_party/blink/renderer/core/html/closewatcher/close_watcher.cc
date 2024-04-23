// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/closewatcher/close_watcher.h"

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
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
  if (watcher_groups_.empty()) {
    auto& host = window_->GetFrame()->GetLocalFrameHostRemote();
    host.SetCloseListener(receiver_.BindNewPipeAndPassRemote(
        window_->GetTaskRunner(TaskType::kMiscPlatformAPI)));
  }

  if (watcher_groups_.size() < allowed_groups_) {
    HeapVector<Member<CloseWatcher>> group;
    group.push_back(watcher);
    watcher_groups_.push_back(group);
  } else {
    // watcher_groups_ should never be empty in this branch, because
    // allowed_groups_ should always be >= 1 and so if watcher_groups_ is empty
    // we would have taken the above branch.
    CHECK(!watcher_groups_.empty());
    watcher_groups_.back().push_back(watcher);
  }

  next_user_interaction_creates_a_new_allowed_group_ = true;
}

void CloseWatcher::WatcherStack::Remove(CloseWatcher* watcher) {
  for (auto& group : watcher_groups_) {
    auto watcher_it = std::find(group.begin(), group.end(), watcher);
    if (watcher_it != group.end()) {
      group.erase(watcher_it);
      if (group.empty()) {
        auto group_it =
            std::find(watcher_groups_.begin(), watcher_groups_.end(), group);
        watcher_groups_.erase(group_it);
      }
      break;
    }
  }

  if (watcher_groups_.empty()) {
    receiver_.reset();
  }
}

void CloseWatcher::WatcherStack::SetHadUserInteraction(
    bool had_user_interaction) {
  if (had_user_interaction) {
    // We don't quite want to give one new allowed group for every user
    // interaction. That would allow "banking" user interactions in a way that's
    // a bit user-hostile: e.g., if the user clicks 20 times in a row with the
    // page not responding at all, then the page would get 20 allowed groups,
    // which at some later time it could use to create 20 close watchers.
    // Instead, each time the user interacts with the page, the page has an
    // *opportunity* to create a new ungrouped close watcher. But if the page
    // doesn't use it, we don't bank the user interaction for the future. This
    // ties close watcher creation to specific user interactions.
    //
    // In short:
    // - OK: user interaction -> create ungrouped close watcher ->
    //       user interaction -> create ungrouped close watcher
    // - Not OK: user interaction x2 -> create ungrouped close watcher x2
    //
    // This does not prevent determined abuse and is not important for upholding
    // our ultimate invariant, of (# of back presses to escape the page) <= (#
    // of user interactions) + 2. A determined abuser will just create one close
    // watcher per user interaction, banking them for future abuse. But it
    // causes more predictable behavior for the normal case, and encourages
    // non-abusive developers to create close watchers directly corresponding to
    // user interactions.
    if (next_user_interaction_creates_a_new_allowed_group_) {
      ++allowed_groups_;
    }
    next_user_interaction_creates_a_new_allowed_group_ = false;
  } else {
    allowed_groups_ = 1;
    next_user_interaction_creates_a_new_allowed_group_ = true;
  }
}

bool CloseWatcher::WatcherStack::CancelEventCanBeCancelable() const {
  return watcher_groups_.size() < allowed_groups_ &&
         window_->GetFrame()->IsHistoryUserActivationActive();
}

void CloseWatcher::WatcherStack::EscapeKeyHandler(KeyboardEvent* event) {
  if (!watcher_groups_.empty() && !event->DefaultHandled() &&
      event->isTrusted() && event->keyCode() == VKEY_ESCAPE) {
    Signal();
  }
}

void CloseWatcher::WatcherStack::Signal() {
  if (!watcher_groups_.empty()) {
    auto& group = watcher_groups_.back();
    for (auto& watcher : base::Reversed(group)) {
      if (!watcher->requestClose()) {
        break;
      }
    }
  }

  if (allowed_groups_ > 1) {
    --allowed_groups_;
  }
}

void CloseWatcher::WatcherStack::Trace(Visitor* visitor) const {
  visitor->Trace(watcher_groups_);
  visitor->Trace(receiver_);
  visitor->Trace(window_);
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

bool CloseWatcher::requestClose() {
  if (IsClosed() || dispatching_cancel_ || !DomWindow()) {
    return true;
  }

  WatcherStack& stack = *DomWindow()->closewatcher_stack();
  Event& cancel_event =
      stack.CancelEventCanBeCancelable()
          ? *Event::CreateCancelable(event_type_names::kCancel)
          : *Event::Create(event_type_names::kCancel);

  {
    base::AutoReset<bool> scoped_committing(&dispatching_cancel_, true);
    DispatchEvent(cancel_event);
  }

  if (cancel_event.defaultPrevented()) {
    if (DomWindow()) {
      DomWindow()->GetFrame()->ConsumeHistoryUserActivation();
    }
    return false;
  }

  close();
  return true;
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

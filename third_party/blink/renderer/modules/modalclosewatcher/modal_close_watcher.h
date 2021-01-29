// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODALCLOSEWATCHER_MODAL_CLOSE_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODALCLOSEWATCHER_MODAL_CLOSE_WATCHER_H_

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ModalCloseWatcher final : public EventTargetWithInlineData,
                                public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ModalCloseWatcher* Create(ScriptState*, ExceptionState&);
  explicit ModalCloseWatcher(LocalDOMWindow*);
  void Trace(Visitor*) const override;

  bool IsClosed() const { return state_ == State::kClosed; }

  void signalClosed();
  void destroy();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforeclose, kBeforeclose)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return ExecutionContextClient::GetExecutionContext();
  }

 private:
  void Close();

  // If multiple ModalCloseWatchers are active in a given window, they form a
  // stack, and a close signal will pop the top watcher. If the stack is empty,
  // the first ModalCloseWatcher is "free", but creating a new
  // ModalCloseWatcher when the stack is non-empty requires a user activation.
  class WatcherStack final : public NativeEventListener,
                             public Supplement<LocalDOMWindow> {
   public:
    static const char kSupplementName[];

    static WatcherStack& From(LocalDOMWindow&);
    explicit WatcherStack(LocalDOMWindow&);

    void Add(ModalCloseWatcher*);
    void Remove(ModalCloseWatcher*);
    bool HasActiveWatcher() { return !watchers_.IsEmpty(); }

    void Trace(Visitor*) const final;

   private:
    // NativeEventListener override:
    void Invoke(ExecutionContext*, Event*) final;

    void Signal() { watchers_.back()->signalClosed(); }

    HeapLinkedHashSet<Member<ModalCloseWatcher>> watchers_;
  };

  enum class State { kActive, kModal, kClosed };
  State state_ = State::kActive;
  bool dispatching_beforeclose_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODALCLOSEWATCHER_MODAL_CLOSE_WATCHER_H_

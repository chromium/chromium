// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/close_watcher/close_listener.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

class CloseWatcherOptions;
class LocalDOMWindow;
class KeyboardEvent;
class HTMLDialogElement;

class CloseWatcher final : public EventTargetWithInlineData,
                           public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static CloseWatcher* Create(ScriptState*,
                              CloseWatcherOptions*,
                              ExceptionState&);

  // We have a few use counters which we trigger only for the <dialog> case,
  // where we're trying to determine whether it's web-compatible or not to use
  // CloseWatcher rules for <dialog>s. (Namely, sometimes closing multiple
  // <dialog>s with a single close signal, and sometimes skipping cancel
  // events.) This argument should be removed after web-compatibility is
  // determined; ultimately the CloseWatcher code should not be aware of the
  // existence of <dialog>, for good layering.
  static CloseWatcher* Create(LocalDOMWindow*,
                              HTMLDialogElement* dialog_for_use_counters);

  explicit CloseWatcher(LocalDOMWindow*,
                        HTMLDialogElement* dialog_for_use_counters);
  void Trace(Visitor*) const override;

  bool IsClosed() const { return state_ == State::kClosed; }
  bool IsGroupedWithPrevious() const { return grouped_with_previous_; }

  void close(bool* cancel_skipped = nullptr);
  void destroy();

  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(close, kClose)

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return ExecutionContextClient::GetExecutionContext();
  }

  // If multiple CloseWatchers are active in a given window, they form a
  // stack, and a close signal will pop the top watcher. If the stack is empty,
  // the first CloseWatcher is "free", but creating a new
  // CloseWatcher when the stack is non-empty requires a user activation.
  class WatcherStack final : public GarbageCollected<WatcherStack>,
                             public mojom::blink::CloseListener {
   public:
    explicit WatcherStack(LocalDOMWindow*);

    void Add(CloseWatcher*);
    void Remove(CloseWatcher*);
    bool HasActiveWatcher() const { return !watchers_.empty(); }
    bool HasConsumedFreeWatcher() const;

    void Trace(Visitor*) const;

    void EscapeKeyHandler(KeyboardEvent*, bool* cancel_skipped);

   private:
    // mojom::blink::CloseListener override:
    void Signal() final;
    void SignalInternal(bool* cancel_skipped);

    HeapLinkedHashSet<Member<CloseWatcher>> watchers_;

    // Holds a pipe which the service uses to notify this object
    // when the idle state has changed.
    HeapMojoReceiver<mojom::blink::CloseListener, WatcherStack> receiver_;
    Member<LocalDOMWindow> window_;
  };

 private:
  static CloseWatcher* CreateInternal(
      LocalDOMWindow*,
      WatcherStack&,
      CloseWatcherOptions*,
      HTMLDialogElement* dialog_for_use_counters);

  enum class State { kActive, kClosed };
  State state_ = State::kActive;
  bool dispatching_cancel_ = false;
  bool grouped_with_previous_ = false;
  bool created_with_user_activation_ = false;
  Member<AbortSignal::AlgorithmHandle> abort_handle_;
  Member<HTMLDialogElement> dialog_for_use_counters_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CLOSEWATCHER_CLOSE_WATCHER_H_

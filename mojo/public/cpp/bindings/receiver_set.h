// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_ptr_impl_ref_traits.h"

namespace mojo {

using ReceiverId = size_t;

template <typename ReceiverType>
struct ReceiverSetTraits;

template <typename Interface, typename ImplRefTraits>
struct ReceiverSetTraits<Receiver<Interface, ImplRefTraits>> {
  using InterfaceType = Interface;
  using PendingType = PendingReceiver<Interface>;
  using ImplPointerType = typename ImplRefTraits::PointerType;
};

template <typename ContextType>
struct ReceiverSetContextTraits {
  using Type = ContextType;

  static constexpr bool SupportsContext() { return true; }
};

template <>
struct ReceiverSetContextTraits<void> {
  // NOTE: This choice of Type only matters insofar as it affects the size of
  // the |context_| field of a ReceiverSetBase::Entry with void context. The
  // context value is never used in this case.
  using Type = bool;

  static constexpr bool SupportsContext() { return false; }
};

// Generic helper used to own a collection of Receiver endpoints. For
// convenience this type automatically manages cleanup of receivers that have
// been disconnected from their remote caller.
//
// Note that this type is not typically used directly by application. Instead,
// prefer to use one of the various aliases (like ReceiverSet) that are based on
// it.
//
// If |ContextType| is non-void, then every added receiver must include a
// context value of that type (when calling |Add()|), and |current_context()|
// will return that value during the extent of any message dispatch or
// disconnection notification pertaining to that specific receiver.
//
// So for example if ContextType is |int| and we call:
//
//   Remote<mojom::Foo> foo1, foo2;
//   ReceiverSet<mojom::Foo> receivers;
//   // Assume |this| is an implementation of mojom::Foo...
//   receivers.Add(this, foo1.BindNewReceiver(), 42);
//   receivers.Add(this, foo2.BindNewReceiver(), 43);
//
//   foo1->DoSomething();
//   foo2->DoSomething();
//
// We can expect two asynchronous calls to |this->DoSomething()|. If that
// method looks at the value of |current_context()|, it will see a value of 42
// while executing the call from |foo1| and a value of 43 while executing the
// call from |foo2|.
//
// Finally, note that ContextType can be any type of thing, including move-only
// objects like std::unique_ptrs.
template <typename ReceiverType, typename ContextType>
class ReceiverSetBase {
 public:
  using Traits = ReceiverSetTraits<ReceiverType>;
  using Interface = typename Traits::InterfaceType;
  using PendingType = typename Traits::PendingType;
  using ImplPointerType = typename Traits::ImplPointerType;
  using ContextTraits = ReceiverSetContextTraits<ContextType>;
  using Context = typename ContextTraits::Type;
  using PreDispatchCallback = base::RepeatingCallback<void(const Context&)>;

  ReceiverSetBase() = default;

  // Sets a callback to be invoked any time a receiver in the set is
  // disconnected. The callback is invoked *after* the receiver in question
  // is removed from the set, and |current_context()| will correspond to the
  // disconnected receiver's context value during the callback if the
  // ContextType is not void.
  void set_disconnect_handler(base::RepeatingClosure handler) {
    disconnect_handler_ = std::move(handler);
    disconnect_with_reason_handler_.Reset();
  }

  // Like above but also provides the reason given for disconnection, if any.
  void set_disconnect_with_reason_handler(
      RepeatingConnectionErrorWithReasonCallback handler) {
    disconnect_with_reason_handler_ = std::move(handler);
    disconnect_handler_.Reset();
  }

  // Adds a new receiver to the set, binding |receiver| to |impl| with no
  // additional context. If |task_runner| is non-null, the receiver's messages
  // will be dispatched to |impl| on that |task_runner|. |task_runner| must run
  // messages on the same sequence that owns this ReceiverSetBase. If
  // |task_runner| is null, the value of
  // |base::SequencedTaskRunnerHandle::Get()| at the time of the |Add()| call
  // will be used to run scheduled tasks for the receiver.
  ReceiverId Add(
      ImplPointerType impl,
      PendingType receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    static_assert(!ContextTraits::SupportsContext(),
                  "Context value required for non-void context type.");
    return AddImpl(std::move(impl), std::move(receiver), false,
                   std::move(task_runner));
  }

  // Adds a new receiver associated with |context|. See above method for all
  // other (identical) details.
  ReceiverId Add(
      ImplPointerType impl,
      PendingType receiver,
      Context context,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddImpl(std::move(impl), std::move(receiver), std::move(context),
                   std::move(task_runner));
  }

  // Removes a receiver from the set. Note that this is safe to call even if the
  // receiver corresponding to |id| has already been removed (will be a no-op).
  //
  // Returns |true| if the receiver was removed and |false| if it didn't exist.
  //
  // A removed receiver is effectively closed and its remote (if any) will be
  // disconnected. No further messages or disconnection notifications will be
  // scheduled or executed for the removed receiver.
  bool Remove(ReceiverId id) {
    auto it = receivers_.find(id);
    if (it == receivers_.end())
      return false;
    receivers_.erase(it);
    return true;
  }

  // Removes all receivers from the set, effectively closing all of them. This
  // ReceiverSet will not schedule or execute any further method invocations or
  // disconnection notifications until a new receiver is added to the set.
  void Clear() { receivers_.clear(); }

  // Predicate to test if a receiver exists in the set.
  //
  // Returns |true| if the receiver is in the set and |false| if not.
  bool HasReceiver(ReceiverId id) const {
    return base::Contains(receivers_, id);
  }

  bool empty() const { return receivers_.empty(); }

  size_t size() const { return receivers_.size(); }

  // Implementations may call this when processing a received method call or
  // disconnection notification. During the extent of method invocation or
  // disconnection notification, this returns the context value associated with
  // the specific receiver which received the method call or disconnection.
  //
  // Each receiver must be associated with a context value when it's added
  // to the set by |Add()|, and this is only supported when ContextType is
  // not void.
  //
  // NOTE: It is important to understand that this must only be called within
  // the stack frame of an actual interface method invocation or disconnect
  // notification scheduled by a receiver. It is a illegal to attempt to call
  // this any other time (e.g., from another async task you post from within a
  // message handler).
  const Context& current_context() const {
    static_assert(ContextTraits::SupportsContext(),
                  "current_context() requires non-void context type.");
    DCHECK(current_context_);
    return *current_context_;
  }

  // Implementations may call this when processing a received method call or
  // disconnection notification. See above note for constraints on usage.
  // This returns the ReceiverId associated with the specific receiver which
  // received the incoming method call or disconnection notification.
  ReceiverId current_receiver() const {
    DCHECK(current_context_);
    return current_receiver_;
  }

  // Reports the currently dispatching Message as bad and removes the receiver
  // which received it. Note that this is only legal to call from directly
  // within the stack frame of an incoming method call. If you need to do
  // asynchronous work before you can determine the legitimacy of a message, use
  // GetBadMessageCallback() and retain its result until you're ready to invoke
  // or discard it.
  void ReportBadMessage(const std::string& error) {
    GetBadMessageCallback().Run(error);
  }

  // Acquires a callback which may be run to report the currently dispatching
  // Message as bad and remove the receiver which received it. Note that this
  // this is only legal to call from directly within the stack frame of an
  // incoming method call, but the returned callback may be called exactly once
  // any time thereafter, as long as the ReceiverSetBase itself hasn't been
  // destroyed yet. If the callback is invoked, it must be done from the same
  // sequence which owns the ReceiverSetBase, and upon invocation it will report
  // the corresponding message as bad.
  ReportBadMessageCallback GetBadMessageCallback() {
    DCHECK(current_context_);
    return base::BindOnce(
        [](ReportBadMessageCallback error_callback,
           base::WeakPtr<ReceiverSetBase> receiver_set, ReceiverId receiver_id,
           const std::string& error) {
          std::move(error_callback).Run(error);
          if (receiver_set)
            receiver_set->Remove(receiver_id);
        },
        mojo::GetBadMessageCallback(), weak_ptr_factory_.GetWeakPtr(),
        current_receiver());
  }

  void FlushForTesting() {
    // We avoid flushing while iterating over |receivers_| because this set
    // may be mutated during individual flush operations.  Instead, snapshot
    // the ReceiverIds first, then iterate over them. This is less efficient,
    // but it's only a testing API. This also allows for correct behavior in
    // reentrant calls to FlushForTesting().
    std::vector<ReceiverId> ids;
    for (const auto& receiver : receivers_)
      ids.push_back(receiver.first);

    auto weak_self = weak_ptr_factory_.GetWeakPtr();
    for (const auto& id : ids) {
      if (!weak_self)
        return;
      auto it = receivers_.find(id);
      if (it != receivers_.end())
        it->second->FlushForTesting();
    }
  }

  // Swaps the interface implementation with a different one, to allow tests
  // to modify behavior.
  //
  // Returns the existing interface implementation to the caller.
  ImplPointerType SwapImplForTesting(ReceiverId id, ImplPointerType new_impl) {
    auto it = receivers_.find(id);
    if (it == receivers_.end())
      return nullptr;

    return it->second->SwapImplForTesting(new_impl);
  }

 private:
  friend class Entry;

  class Entry {
   public:
    Entry(ImplPointerType impl,
          PendingType receiver,
          ReceiverSetBase* receiver_set,
          ReceiverId receiver_id,
          Context context,
          scoped_refptr<base::SequencedTaskRunner> task_runner)
        : receiver_(std::move(impl),
                    std::move(receiver),
                    std::move(task_runner)),
          receiver_set_(receiver_set),
          receiver_id_(receiver_id),
          context_(std::move(context)) {
      receiver_.SetFilter(std::make_unique<DispatchFilter>(this));
      receiver_.set_disconnect_with_reason_handler(
          base::BindOnce(&Entry::OnDisconnect, base::Unretained(this)));
    }

    ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
      return receiver_.SwapImplForTesting(new_impl);
    }

    void FlushForTesting() { receiver_.FlushForTesting(); }

   private:
    class DispatchFilter : public MessageFilter {
     public:
      explicit DispatchFilter(Entry* entry) : entry_(entry) {}
      ~DispatchFilter() override {}

     private:
      // MessageFilter:
      bool WillDispatch(Message* message) override {
        entry_->WillDispatch();
        return true;
      }

      void DidDispatchOrReject(Message* message, bool accepted) override {}

      Entry* entry_;

      DISALLOW_COPY_AND_ASSIGN(DispatchFilter);
    };

    void WillDispatch() {
      receiver_set_->SetDispatchContext(&context_, receiver_id_);
    }

    void OnDisconnect(uint32_t custom_reason_code,
                      const std::string& description) {
      WillDispatch();
      receiver_set_->OnDisconnect(receiver_id_, custom_reason_code,
                                  description);
    }

    ReceiverType receiver_;
    ReceiverSetBase* const receiver_set_;
    const ReceiverId receiver_id_;
    Context const context_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void SetDispatchContext(const Context* context, ReceiverId receiver_id) {
    current_context_ = context;
    current_receiver_ = receiver_id;
  }

  ReceiverId AddImpl(ImplPointerType impl,
                     PendingType receiver,
                     Context context,
                     scoped_refptr<base::SequencedTaskRunner> task_runner) {
    ReceiverId id = next_receiver_id_++;
    DCHECK_GE(next_receiver_id_, 0u);
    auto entry =
        std::make_unique<Entry>(std::move(impl), std::move(receiver), this, id,
                                std::move(context), std::move(task_runner));
    receivers_.insert(std::make_pair(id, std::move(entry)));
    return id;
  }

  void OnDisconnect(ReceiverId id,
                    uint32_t custom_reason_code,
                    const std::string& description) {
    auto it = receivers_.find(id);
    DCHECK(it != receivers_.end());

    // We keep the Entry alive throughout error dispatch.
    std::unique_ptr<Entry> entry = std::move(it->second);
    receivers_.erase(it);

    if (disconnect_handler_)
      disconnect_handler_.Run();
    else if (disconnect_with_reason_handler_)
      disconnect_with_reason_handler_.Run(custom_reason_code, description);
  }

  base::RepeatingClosure disconnect_handler_;
  RepeatingConnectionErrorWithReasonCallback disconnect_with_reason_handler_;
  ReceiverId next_receiver_id_ = 0;
  std::map<ReceiverId, std::unique_ptr<Entry>> receivers_;
  const Context* current_context_ = nullptr;
  ReceiverId current_receiver_;
  base::WeakPtrFactory<ReceiverSetBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReceiverSetBase);
};

// Common helper for a set of Receivers which do not own their implementation.
template <typename Interface, typename ContextType = void>
using ReceiverSet = ReceiverSetBase<Receiver<Interface>, ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_

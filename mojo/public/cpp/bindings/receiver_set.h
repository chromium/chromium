// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/bindings/unique_ptr_impl_ref_traits.h"

namespace mojo {

namespace test {
class ReceiverSetStaticAssertTests;
}

using ReceiverId = uint64_t;

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
  struct Empty {};

  using Type = Empty;

  static constexpr bool SupportsContext() { return false; }
};

// Shared base class owning specific type-agnostic ReceiverSet state and logic.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ReceiverSetState {
 public:
  class ReceiverState {
   public:
    virtual ~ReceiverState() = default;
    virtual const void* GetContext() const = 0;
    virtual void* GetContext() = 0;
    virtual void InstallDispatchHooks(
        std::unique_ptr<MessageFilter> filter,
        RepeatingConnectionErrorWithReasonCallback disconnect_handler) = 0;
    virtual void FlushForTesting() = 0;
    virtual void ResetWithReason(uint32_t custom_reason_code,
                                 const std::string& description) = 0;
  };

  class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) Entry {
   public:
    Entry(ReceiverSetState& state,
          ReceiverId id,
          std::unique_ptr<ReceiverState> receiver,
          std::unique_ptr<MessageFilter> filter);
    ~Entry();

    ReceiverState& receiver() { return *receiver_; }

   private:
    class DispatchFilter;

    void WillDispatch();
    void DidDispatchOrReject();
    void OnDisconnect(uint32_t custom_reason_code,
                      const std::string& description);

    // RAW_PTR_EXCLUSION: Binary size increase.
    RAW_PTR_EXCLUSION ReceiverSetState& state_;
    const ReceiverId id_;
    const std::unique_ptr<ReceiverState> receiver_;
  };

  using EntryMap = std::map<ReceiverId, std::unique_ptr<Entry>>;

  ReceiverSetState();
  ReceiverSetState(const ReceiverSetState&) = delete;
  ReceiverSetState& operator=(const ReceiverSetState&) = delete;
  ~ReceiverSetState();

  EntryMap& entries() { return entries_; }
  const EntryMap& entries() const { return entries_; }

  const void* current_context() const {
    DCHECK(current_context_);
    return current_context_;
  }

  void* current_context() {
    DCHECK(current_context_);
    return current_context_;
  }

  ReceiverId current_receiver() const {
    DCHECK(current_context_);
    return current_receiver_;
  }

  void set_disconnect_handler(base::RepeatingClosure handler);
  void set_disconnect_with_reason_handler(
      RepeatingConnectionErrorWithReasonCallback handler);

  ReportBadMessageCallback GetBadMessageCallback();
  ReceiverId Add(std::unique_ptr<ReceiverState> receiver,
                 std::unique_ptr<MessageFilter> filter);
  bool Remove(ReceiverId id);
  bool RemoveWithReason(ReceiverId id,
                        uint32_t custom_reason_code,
                        const std::string& description);
  void FlushForTesting();
  void SetDispatchContext(void* context, ReceiverId receiver_id);
  void OnDisconnect(ReceiverId id,
                    uint32_t custom_reason_code,
                    const std::string& description);

 private:
  base::RepeatingClosure disconnect_handler_;
  RepeatingConnectionErrorWithReasonCallback disconnect_with_reason_handler_;
  ReceiverId next_receiver_id_ = 0;
  EntryMap entries_;
  raw_ptr<void, DanglingUntriaged> current_context_ = nullptr;
  ReceiverId current_receiver_;
  base::WeakPtrFactory<ReceiverSetState> weak_ptr_factory_{this};
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
// RuntimeFeature guarded receivers should only be added to a set if they are
// enabled - if an interface is feature guarded validate the enabled state of
// the corresponding feature before calling Add().
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
  ReceiverSetBase(const ReceiverSetBase&) = delete;
  ReceiverSetBase& operator=(const ReceiverSetBase&) = delete;

  // Sets a callback to be invoked any time a receiver in the set is
  // disconnected. The callback is invoked *after* the receiver in question
  // is removed from the set, and |current_context()| will correspond to the
  // disconnected receiver's context value during the callback if the
  // ContextType is not void.
  void set_disconnect_handler(base::RepeatingClosure handler) {
    state_.set_disconnect_handler(std::move(handler));
  }

  // Like above but also provides the reason given for disconnection, if any.
  void set_disconnect_with_reason_handler(
      RepeatingConnectionErrorWithReasonCallback handler) {
    state_.set_disconnect_with_reason_handler(std::move(handler));
  }

  // Adds a new receiver to the set, binding |receiver| to |impl| with no
  // additional context. If |task_runner| is non-null, the receiver's messages
  // will be dispatched to |impl| on that |task_runner|. |task_runner| must run
  // messages on the same sequence that owns this ReceiverSetBase. If
  // |task_runner| is null, the value of
  // |base::SequencedTaskRunner::GetCurrentDefault()| at the time of the |Add()|
  // call will be used to run scheduled tasks for the receiver.
  ReceiverId Add(ImplPointerType impl,
                 PendingType receiver,
                 scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(!internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    return AddImpl(std::move(impl), std::move(receiver), {},
                   std::move(task_runner), /*filter=*/nullptr)
        .value();
  }

  // Like Add() but allows an interface with a runtime enabled feature to be
  // provided - if the feature is enabled or the interface does not have a
  // RuntimeFeature attribute this behaves exactly like Add() and always returns
  // a .value(). If the feature is disabled this will DCHECK in developer builds
  // and return nullopt in production - `impl` will be immediately destroyed.
  std::optional<ReceiverId> Add(
      ImplPointerType impl,
      PendingType receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    return AddImpl(std::move(impl), std::move(receiver), {},
                   std::move(task_runner), /*filter=*/nullptr);
  }

  // Adds a new receiver associated with |context|. See above method for all
  // other (identical) details.
  ReceiverId Add(ImplPointerType impl,
                 PendingType receiver,
                 Context context,
                 scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(!internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddImpl(std::move(impl), std::move(receiver), std::move(context),
                   std::move(task_runner), /*filter=*/nullptr)
        .value();
  }

  // See above.
  std::optional<ReceiverId> Add(
      ImplPointerType impl,
      PendingType receiver,
      Context context,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddImpl(std::move(impl), std::move(receiver), std::move(context),
                   std::move(task_runner), /*filter=*/nullptr);
  }

  // Adds a new receiver associated with |context| and which uses the
  // MessageFilter |filter|. See above for all other details.
  ReceiverId Add(ImplPointerType impl,
                 PendingType receiver,
                 Context context,
                 std::unique_ptr<MessageFilter> filter,
                 scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(!internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddImpl(std::move(impl), std::move(receiver), std::move(context),
                   std::move(task_runner), std::move(filter))
        .value();
  }

  // See above.
  std::optional<ReceiverId> Add(
      ImplPointerType impl,
      PendingType receiver,
      Context context,
      std::unique_ptr<MessageFilter> filter,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddImpl(std::move(impl), std::move(receiver), std::move(context),
                   std::move(task_runner), std::move(filter));
  }

  // Removes a receiver from the set. Note that this is safe to call even if the
  // receiver corresponding to |id| has already been removed (will be a no-op).
  //
  // Returns |true| if the receiver was removed and |false| if it didn't exist.
  //
  // A removed receiver is effectively closed and its remote (if any) will be
  // disconnected. No further messages or disconnection notifications will be
  // scheduled or executed for the removed receiver.
  bool Remove(ReceiverId id) { return state_.Remove(id); }
  // Similar to the method above, but also specifies a disconnect reason.
  bool RemoveWithReason(ReceiverId id,
                        uint32_t custom_reason_code,
                        const std::string& description) {
    return state_.RemoveWithReason(id, custom_reason_code, description);
  }

  // Unbinds and takes all receivers in this set.
  std::vector<PendingType> TakeReceivers() {
    ReceiverSetState::EntryMap entries;
    std::swap(state_.entries(), entries);
    std::vector<PendingType> pending_receivers;
    for (auto& entry : entries) {
      ReceiverEntry& receiver =
          static_cast<ReceiverEntry&>(entry.second->receiver());
      pending_receivers.push_back(receiver.Unbind());
    }
    return pending_receivers;
  }

  // Removes all receivers from the set, effectively closing all of them. This
  // ReceiverSet will not schedule or execute any further method invocations or
  // disconnection notifications until a new receiver is added to the set.
  void Clear() { state_.entries().clear(); }
  // Similar to the method above, but also specifies a disconnect reason.
  void ClearWithReason(uint32_t custom_reason_code,
                       const std::string& description) {
    for (auto& entry : state_.entries())
      entry.second->receiver().ResetWithReason(custom_reason_code, description);

    Clear();
  }

  // Predicate to test if a receiver exists in the set.
  //
  // Returns |true| if the receiver is in the set and |false| if not.
  bool HasReceiver(ReceiverId id) const {
    return base::Contains(state_.entries(), id);
  }

  // Returns a pointer to the context associated with a receiver.
  //
  // Returns |nullptr| if the receiver is not in the set.
  Context* GetContext(ReceiverId id) const {
    static_assert(ContextTraits::SupportsContext(),
                  "GetContext() requires non-void context type.");
    auto it = state_.entries().find(id);
    if (it == state_.entries().end()) {
      return nullptr;
    }
    return static_cast<Context*>(it->second->receiver().GetContext());
  }

  // Returns a map from the ID to the associated context for each receiver in
  // the set.
  std::map<ReceiverId, Context*> GetAllContexts() const {
    static_assert(ContextTraits::SupportsContext(),
                  "GetAllContexts() requires non-void context type.");
    std::map<ReceiverId, Context*> contexts;
    for (const auto& [receiver_id, entry] : state_.entries()) {
      contexts[receiver_id] =
          static_cast<Context*>(entry->receiver().GetContext());
    }
    return contexts;
  }

  bool empty() const { return state_.entries().empty(); }

  size_t size() const { return state_.entries().size(); }

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
    return *static_cast<const Context*>(state_.current_context());
  }

  // Like `current_context() const`, but returns non-const reference to the
  // context value.
  Context& current_context() {
    static_assert(ContextTraits::SupportsContext(),
                  "current_context() requires non-void context type.");
    return *static_cast<Context*>(state_.current_context());
  }

  // Implementations may call this when processing a received method call or
  // disconnection notification. See above note for constraints on usage.
  // This returns the ReceiverId associated with the specific receiver which
  // received the incoming method call or disconnection notification.
  ReceiverId current_receiver() const { return state_.current_receiver(); }

  // Reports the currently dispatching Message as bad and removes the receiver
  // which received it. Note that this is only legal to call from directly
  // within the stack frame of an incoming method call. If you need to do
  // asynchronous work before you can determine the legitimacy of a message, use
  // GetBadMessageCallback() and retain its result until you're ready to invoke
  // or discard it.
  NOT_TAIL_CALLED void ReportBadMessage(const std::string& error) {
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
    return state_.GetBadMessageCallback();
  }

  void FlushForTesting() { state_.FlushForTesting(); }

  // Swaps the interface implementation with a different one, to allow tests
  // to modify behavior.
  //
  // Returns the existing interface implementation to the caller.
  //
  // The caller needs to guarantee that `new_impl` will live longer than
  // `this` ReceiverSet.  One way to achieve this is to store the returned
  // `old_impl` and swap it back in when `new_impl` is getting destroyed.
  // Test code should prefer using `mojo::test::ScopedSwapImplForTesting` if
  // possible.
  [[nodiscard]] ImplPointerType SwapImplForTesting(ReceiverId id,
                                                   ImplPointerType new_impl) {
    auto it = state_.entries().find(id);
    if (it == state_.entries().end())
      return nullptr;

    ReceiverEntry& entry = static_cast<ReceiverEntry&>(it->second->receiver());
    return entry.SwapImplForTesting(std::move(new_impl));
  }

 private:
  friend test::ReceiverSetStaticAssertTests;

  class ReceiverEntry : public ReceiverSetState::ReceiverState {
   public:
    ReceiverEntry(ImplPointerType impl,
                  PendingType receiver,
                  Context context,
                  scoped_refptr<base::SequencedTaskRunner> task_runner)
        : receiver_(std::move(impl),
                    std::move(receiver),
                    std::move(task_runner)),
          context_(std::move(context)) {}
    ReceiverEntry(const ReceiverEntry&) = delete;
    ReceiverEntry& operator=(const ReceiverEntry&) = delete;
    ~ReceiverEntry() override = default;

    // ReceiverSetState::ReceiverState:
    const void* GetContext() const override { return &context_; }
    void* GetContext() override { return &context_; }

    void InstallDispatchHooks(std::unique_ptr<MessageFilter> filter,
                              RepeatingConnectionErrorWithReasonCallback
                                  disconnect_handler) override {
      receiver_.SetFilter(std::move(filter));
      receiver_.set_disconnect_with_reason_handler(
          std::move(disconnect_handler));
    }

    void FlushForTesting() override { receiver_.FlushForTesting(); }

    void ResetWithReason(uint32_t custom_reason_code,
                         const std::string& description) override {
      receiver_.ResetWithReason(custom_reason_code, description);
    }

    ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
      return receiver_.SwapImplForTesting(std::move(new_impl));
    }

    PendingType Unbind() { return receiver_.Unbind(); }

   private:
    ReceiverType receiver_;
    NO_UNIQUE_ADDRESS Context context_;
  };

  std::optional<ReceiverId> AddImpl(
      ImplPointerType impl,
      PendingType receiver,
      Context context,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MessageFilter> filter) {
    DCHECK(receiver.is_valid());
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return std::nullopt;
    }
    return state_.Add(std::make_unique<ReceiverEntry>(
                          std::move(impl), std::move(receiver),
                          std::move(context), std::move(task_runner)),
                      std::move(filter));
  }

  ReceiverSetState state_;
};

// Common helper for a set of Receivers which do not own their implementation.
template <typename Interface, typename ContextType = void>
using ReceiverSet = ReceiverSetBase<Receiver<Interface>, ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_RECEIVER_SET_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

template <typename BindingType>
struct BindingSetTraits;

template <typename Interface, typename ImplRefTraits>
struct BindingSetTraits<Binding<Interface, ImplRefTraits>> {
  using ProxyType = InterfacePtr<Interface>;
  using RequestType = InterfaceRequest<Interface>;
  using BindingType = Binding<Interface, ImplRefTraits>;
  using ImplPointerType = typename BindingType::ImplPointerType;

  static RequestType MakeRequest(ProxyType* proxy) {
    return mojo::MakeRequest(proxy);
  }
};

using BindingId = size_t;

template <typename ContextType>
struct BindingSetContextTraits {
  using Type = ContextType;

  static constexpr bool SupportsContext() { return true; }
};

template <>
struct BindingSetContextTraits<void> {
  // NOTE: This choice of Type only matters insofar as it affects the size of
  // the |context_| field of a BindingSetBase::Entry with void context. The
  // context value is never used in this case.
  using Type = bool;

  static constexpr bool SupportsContext() { return false; }
};

// Generic definition used for BindingSet and AssociatedBindingSet to own a
// collection of bindings which point to the same implementation.
//
// If |ContextType| is non-void, then every added binding must include a context
// value of that type, and |dispatch_context()| will return that value during
// the extent of any message dispatch targeting that specific binding.
template <typename Interface, typename BindingType, typename ContextType>
class BindingSetBase {
 public:
  using ContextTraits = BindingSetContextTraits<ContextType>;
  using Context = typename ContextTraits::Type;
  using Traits = BindingSetTraits<BindingType>;
  using ProxyType = typename Traits::ProxyType;
  using RequestType = typename Traits::RequestType;
  using ImplPointerType = typename Traits::ImplPointerType;

  BindingSetBase() {}

  void set_connection_error_handler(base::RepeatingClosure error_handler) {
    error_handler_ = std::move(error_handler);
    error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      RepeatingConnectionErrorWithReasonCallback error_handler) {
    error_with_reason_handler_ = std::move(error_handler);
    error_handler_.Reset();
  }

  // Adds a new binding to the set which binds |request| to |impl| with no
  // additional context.
  BindingId AddBinding(
      ImplPointerType impl,
      RequestType request,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    static_assert(!ContextTraits::SupportsContext(),
                  "Context value required for non-void context type.");
    return AddBindingImpl(std::move(impl), std::move(request), false,
                          std::move(task_runner));
  }

  // Adds a new binding associated with |context|.
  BindingId AddBinding(
      ImplPointerType impl,
      RequestType request,
      Context context,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    static_assert(ContextTraits::SupportsContext(),
                  "Context value unsupported for void context type.");
    return AddBindingImpl(std::move(impl), std::move(request),
                          std::move(context), std::move(task_runner));
  }

  // Removes a binding from the set. Note that this is safe to call even if the
  // binding corresponding to |id| has already been removed.
  //
  // Returns |true| if the binding was removed and |false| if it didn't exist.
  bool RemoveBinding(BindingId id) {
    auto it = bindings_.find(id);
    if (it == bindings_.end())
      return false;
    bindings_.erase(it);
    return true;
  }

  // Predicate to test if a binding exists in the set.
  //
  // Returns |true| if the binding is in the set and |false| if not.
  bool HasBinding(BindingId id) const { return base::Contains(bindings_, id); }

  // Swaps the interface implementation with a different one, to allow tests
  // to modify behavior.
  //
  // Returns the existing interface implementation to the caller.
  ImplPointerType SwapImplForTesting(BindingId id, ImplPointerType new_impl) {
    auto it = bindings_.find(id);
    if (it == bindings_.end())
      return nullptr;

    return it->second->SwapImplForTesting(new_impl);
  }

  void CloseAllBindings() { bindings_.clear(); }

  bool empty() const { return bindings_.empty(); }

  size_t size() const { return bindings_.size(); }

  // Implementations may call this when processing a dispatched message or
  // error. During the extent of message or error dispatch, this will return the
  // context associated with the specific binding which received the message or
  // error. Use AddBinding() to associated a context with a specific binding.
  const Context& dispatch_context() const {
    static_assert(ContextTraits::SupportsContext(),
                  "dispatch_context() requires non-void context type.");
    DCHECK(dispatch_context_);
    return *dispatch_context_;
  }

  // Implementations may call this when processing a dispatched message or
  // error. During the extent of message or error dispatch, this will return the
  // BindingId of the specific binding which received the message or error.
  BindingId dispatch_binding() const {
    DCHECK(dispatch_context_);
    return dispatch_binding_;
  }

  // Reports the currently dispatching Message as bad and closes the binding the
  // message was received from. Note that this is only legal to call from
  // directly within the stack frame of a message dispatch. If you need to do
  // asynchronous work before you can determine the legitimacy of a message, use
  // GetBadMessageCallback() and retain its result until you're ready to invoke
  // or discard it.
  void ReportBadMessage(const std::string& error) {
    GetBadMessageCallback().Run(error);
  }

  // Acquires a callback which may be run to report the currently dispatching
  // Message as bad and close the binding the message was received from. Note
  // that this is only legal to call from directly within the stack frame of a
  // message dispatch, but the returned callback may be called exactly once any
  // time thereafter as long as the binding set itself hasn't been destroyed yet
  // to report the message as bad. This may only be called once per message.
  // The returned callback must be called on the BindingSet's own sequence.
  ReportBadMessageCallback GetBadMessageCallback() {
    DCHECK(dispatch_context_);
    return base::BindOnce(
        [](ReportBadMessageCallback error_callback,
           base::WeakPtr<BindingSetBase> binding_set, BindingId binding_id,
           const std::string& error) {
          std::move(error_callback).Run(error);
          if (binding_set)
            binding_set->RemoveBinding(binding_id);
        },
        mojo::GetBadMessageCallback(), weak_ptr_factory_.GetWeakPtr(),
        dispatch_binding());
  }

  void FlushForTesting() {
    DCHECK(!is_flushing_);
    is_flushing_ = true;
    for (auto& binding : bindings_)
      if (binding.second)
        binding.second->FlushForTesting();
    is_flushing_ = false;
    // Clean up any bindings that were destroyed.
    for (auto it = bindings_.begin(); it != bindings_.end();) {
      if (!it->second)
        it = bindings_.erase(it);
      else
        ++it;
    }
  }

 private:
  friend class Entry;

  class Entry {
   public:
    Entry(ImplPointerType impl,
          RequestType request,
          BindingSetBase* binding_set,
          BindingId binding_id,
          Context context,
          scoped_refptr<base::SequencedTaskRunner> task_runner)
        : binding_(std::move(impl), std::move(request), std::move(task_runner)),
          binding_set_(binding_set),
          binding_id_(binding_id),
          context_(std::move(context)) {
      binding_.SetFilter(std::make_unique<DispatchFilter>(this));
      binding_.set_connection_error_with_reason_handler(
          base::BindOnce(&Entry::OnConnectionError, base::Unretained(this)));
    }

    void FlushForTesting() { binding_.FlushForTesting(); }

    ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
      return binding_.SwapImplForTesting(new_impl);
    }

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
      binding_set_->SetDispatchContext(&context_, binding_id_);
    }

    void OnConnectionError(uint32_t custom_reason,
                           const std::string& description) {
      WillDispatch();
      binding_set_->OnConnectionError(binding_id_, custom_reason, description);
    }

    BindingType binding_;
    BindingSetBase* const binding_set_;
    const BindingId binding_id_;
    Context const context_;

    DISALLOW_COPY_AND_ASSIGN(Entry);
  };

  void SetDispatchContext(const Context* context, BindingId binding_id) {
    dispatch_context_ = context;
    dispatch_binding_ = binding_id;
  }

  BindingId AddBindingImpl(
      ImplPointerType impl,
      RequestType request,
      Context context,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    BindingId id = next_binding_id_++;
    DCHECK_GE(next_binding_id_, 0u);
    auto entry =
        std::make_unique<Entry>(std::move(impl), std::move(request), this, id,
                                std::move(context), std::move(task_runner));
    bindings_.insert(std::make_pair(id, std::move(entry)));
    return id;
  }

  void OnConnectionError(BindingId id,
                         uint32_t custom_reason,
                         const std::string& description) {
    auto it = bindings_.find(id);
    DCHECK(it != bindings_.end());

    // We keep the Entry alive throughout error dispatch.
    std::unique_ptr<Entry> entry = std::move(it->second);
    if (!is_flushing_)
      bindings_.erase(it);

    if (error_handler_) {
      error_handler_.Run();
    } else if (error_with_reason_handler_) {
      error_with_reason_handler_.Run(custom_reason, description);
    }
  }

  base::RepeatingClosure error_handler_;
  RepeatingConnectionErrorWithReasonCallback error_with_reason_handler_;
  BindingId next_binding_id_ = 0;
  std::map<BindingId, std::unique_ptr<Entry>> bindings_;
  bool is_flushing_ = false;
  const Context* dispatch_context_ = nullptr;
  BindingId dispatch_binding_;
  base::WeakPtrFactory<BindingSetBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BindingSetBase);
};

template <typename Interface, typename ContextType = void>
using BindingSet = BindingSetBase<Interface, Binding<Interface>, ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_BINDING_SET_H_

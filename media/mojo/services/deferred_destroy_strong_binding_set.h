// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_STRONG_BINDING_SET_H_
#define MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_STRONG_BINDING_SET_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace media {

// A class that can be deferred destroyed by its owner. For example, when used
// in DeferredDestroyStrongBindingSet.
template <typename Interface>
class DeferredDestroy : public Interface {
 public:
  // Runs the |destroy_cb| to notify that it's okay to destroy |this|. The
  // callback can be called synchronously. |this| will always be destroyed
  // asynchronously after running |destroy_cb| to avoid reentrance issues.
  virtual void OnDestroyPending(base::OnceClosure destroy_cb) = 0;
};

// Similar to mojo::StrongBindingSet, but provide a way to defer the destruction
// of the interface implementation:
// - When connection error happended on a binding, the binding is immediately
// destroyed and removed from the set. The interface implementation will be
// destroyed when the DestroyCallback is called.
// - When the DeferredDestroyStrongBindingSet is destructed, all outstanding
// bindings and interface implementations in the set are destroyed immediately
// without any deferral.
template <typename Interface>
class DeferredDestroyStrongBindingSet {
 public:
  // Converts a delete callback to a deleter. If the callback is null or has
  // been cancelled, callback bound with invalidated weak pointer, the pointer
  // will be deleted with "delete" immediately.
  class Deleter {
   public:
    using DeleteCallback =
        base::RepeatingCallback<void(std::unique_ptr<Interface>)>;

    Deleter() = default;
    explicit Deleter(DeleteCallback delete_cb)
        : delete_cb_(std::move(delete_cb)) {}

    void operator()(Interface* p) {
      // Immediately wrap |p| into a unique_ptr to avoid any potential leak.
      auto ptr = base::WrapUnique<Interface>(p);

      // Can be cancelled during DeferredDestroyStrongBindingSet destruction.
      if (delete_cb_ && !delete_cb_.IsCancelled())
        delete_cb_.Run(std::move(ptr));
      else
        ptr.reset();
    }

   private:
    DeleteCallback delete_cb_;
  };

  DeferredDestroyStrongBindingSet() {}

  void AddBinding(std::unique_ptr<DeferredDestroy<Interface>> impl,
                  mojo::InterfaceRequest<Interface> request) {
    // Wrap the pointer into a unique_ptr with a deleter.
    Deleter deleter(
        base::BindRepeating(&DeferredDestroyStrongBindingSet::OnBindingRemoved,
                            weak_factory_.GetWeakPtr()));
    std::unique_ptr<Interface, Deleter> impl_with_deleter(impl.release(),
                                                          deleter);

    bindings_.AddBinding(std::move(impl_with_deleter), std::move(request));
  }

  // TODO(xhwang): Add RemoveBinding() if needed.

  void CloseAllBindings() {
    weak_factory_.InvalidateWeakPtrs();
    bindings_.CloseAllBindings();
    unbound_impls_.clear();
  }

  bool empty() const { return bindings_.empty(); }

  size_t size() const { return bindings_.size(); }

  size_t unbound_size() const { return unbound_impls_.size(); }

 private:
  void OnBindingRemoved(std::unique_ptr<Interface> ptr) {
    DVLOG(1) << __func__;

    id_++;

    // The cast is safe since AddBinding() takes DeferredDestroy<Interface>.
    auto* impl_ptr = static_cast<DeferredDestroy<Interface>*>(ptr.get());

    // Put the |ptr| in the map before calling OnDestroyPending() because the
    // callback could be called synchronously.
    unbound_impls_[id_] = std::move(ptr);

    // Use BindToCurrentLoop() to force post the destroy callback. This is
    // needed because the callback may be called directly in the same stack
    // where the implemenation is being destroyed.
    impl_ptr->OnDestroyPending(BindToCurrentLoop(
        base::BindOnce(&DeferredDestroyStrongBindingSet::OnDestroyable,
                       weak_factory_.GetWeakPtr(), id_)));
  }

  void OnDestroyable(int id) {
    DVLOG(1) << __func__;
    unbound_impls_.erase(id);
  }

  uint32_t id_ = 0;
  std::map<uint32_t, std::unique_ptr<Interface>> unbound_impls_;
  mojo::StrongBindingSet<Interface, void, Deleter> bindings_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DeferredDestroyStrongBindingSet> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeferredDestroyStrongBindingSet);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_STRONG_BINDING_SET_H_

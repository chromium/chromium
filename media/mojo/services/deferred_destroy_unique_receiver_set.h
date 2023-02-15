// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_UNIQUE_RECEIVER_SET_H_
#define MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_UNIQUE_RECEIVER_SET_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {

// A class that can be deferred destroyed by its owner. For example, when used
// in DeferredDestroyUniqueReceiverSet.
template <typename Interface>
class DeferredDestroy : public Interface {
 public:
  // Runs the |destroy_cb| to notify that it's okay to destroy |this|. The
  // callback can be called synchronously. |this| will always be destroyed
  // asynchronously after running |destroy_cb| to avoid reentrance issues.
  virtual void OnDestroyPending(base::OnceClosure destroy_cb) = 0;
};

// Similar to mojo::UniqueReceiverSet, but provide a way to defer the
// destruction of the interface implementation:
// - When disconnection happened on a receiver, the receiver is immediately
// destroyed and removed from the set. The interface implementation will be
// destroyed when the DestroyCallback is called.
// - When the DeferredDestroyUniqueReceiverSet is destructed, all outstanding
// receivers and interface implementations in the set are destroyed immediately
// without any deferral.
template <typename Interface>
class DeferredDestroyUniqueReceiverSet {
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

      // Can be cancelled during DeferredDestroyUniqueReceiverSet destruction.
      if (delete_cb_ && !delete_cb_.IsCancelled())
        delete_cb_.Run(std::move(ptr));
      else
        ptr.reset();
    }

   private:
    DeleteCallback delete_cb_;
  };

  DeferredDestroyUniqueReceiverSet() {}

  DeferredDestroyUniqueReceiverSet(const DeferredDestroyUniqueReceiverSet&) =
      delete;
  DeferredDestroyUniqueReceiverSet& operator=(
      const DeferredDestroyUniqueReceiverSet&) = delete;

  void Add(std::unique_ptr<DeferredDestroy<Interface>> impl,
           mojo::PendingReceiver<Interface> receiver) {
    // Wrap the pointer into a unique_ptr with a deleter.
    Deleter deleter(base::BindRepeating(
        &DeferredDestroyUniqueReceiverSet::OnReceiverRemoved,
        weak_factory_.GetWeakPtr()));
    std::unique_ptr<Interface, Deleter> impl_with_deleter(impl.release(),
                                                          deleter);

    receivers_.Add(std::move(impl_with_deleter), std::move(receiver));
  }

  // TODO(xhwang): Add RemoveReceiver() if needed.

  void CloseAllReceivers() {
    weak_factory_.InvalidateWeakPtrs();
    receivers_.Clear();
    unbound_impls_.clear();
  }

  bool empty() const { return receivers_.empty(); }

  size_t size() const { return receivers_.size(); }

  size_t unbound_size() const { return unbound_impls_.size(); }

 private:
  void OnReceiverRemoved(std::unique_ptr<Interface> ptr) {
    DVLOG(1) << __func__;

    id_++;

    // The cast is safe since AddReceiver() takes DeferredDestroy<Interface>.
    auto* impl_ptr = static_cast<DeferredDestroy<Interface>*>(ptr.get());

    // Put the |ptr| in the map before calling OnDestroyPending() because the
    // callback could be called synchronously.
    unbound_impls_[id_] = std::move(ptr);

    // Use base::BindPostTaskToCurrentDefault() to force post the destroy
    // callback. This is needed because the callback may be called directly in
    // the same stack where the implementation is being destroyed.
    impl_ptr->OnDestroyPending(base::BindPostTaskToCurrentDefault(
        base::BindOnce(&DeferredDestroyUniqueReceiverSet::OnDestroyable,
                       weak_factory_.GetWeakPtr(), id_)));
  }

  void OnDestroyable(int id) {
    DVLOG(1) << __func__;
    unbound_impls_.erase(id);
  }

  uint32_t id_ = 0;
  std::map<uint32_t, std::unique_ptr<Interface>> unbound_impls_;
  mojo::UniqueReceiverSet<Interface, void, Deleter> receivers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DeferredDestroyUniqueReceiverSet> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_DEFERRED_DESTROY_UNIQUE_RECEIVER_SET_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"

namespace mojo {

using InterfacePtrSetElementId = size_t;

namespace internal {

// TODO(https://crbug.com/965668): This class should be rewritten to be
// structured similarly to BindingSet if possible, with PtrSet owning its
// Elements and those Elements calling back into PtrSet on connection
// error.
template <typename Interface, template <typename> class Ptr>
class PtrSet {
 public:
  PtrSet() {}
  ~PtrSet() { CloseAll(); }

  InterfacePtrSetElementId AddPtr(Ptr<Interface> ptr) {
    InterfacePtrSetElementId id = next_ptr_id_++;
    auto weak_interface_ptr = new Element(std::move(ptr));
    ptrs_.emplace(std::piecewise_construct, std::forward_as_tuple(id),
                  std::forward_as_tuple(weak_interface_ptr->GetWeakPtr()));
    ClearNullPtrs();
    return id;
  }

  template <typename FunctionType>
  void ForAllPtrs(FunctionType function) {
    for (const auto& it : ptrs_) {
      if (it.second)
        function(it.second->get());
    }
    ClearNullPtrs();
  }

  void CloseAll() {
    for (const auto& it : ptrs_) {
      if (it.second)
        it.second->Close();
    }
    ptrs_.clear();
  }

  bool empty() const { return ptrs_.empty(); }

  // Calls FlushForTesting on all Ptrs sequentially. Since each call is a
  // blocking operation, may be very slow as the number of pointers increases.
  void FlushForTesting() {
    for (const auto& it : ptrs_) {
      if (it.second)
        it.second->FlushForTesting();
    }
    ClearNullPtrs();
  }

  bool HasPtr(InterfacePtrSetElementId id) {
    return ptrs_.find(id) != ptrs_.end();
  }

  Ptr<Interface> RemovePtr(InterfacePtrSetElementId id) {
    auto it = ptrs_.find(id);
    if (it == ptrs_.end())
      return Ptr<Interface>();
    Ptr<Interface> ptr;
    if (it->second) {
      ptr = it->second->Take();
      delete it->second.get();
    }
    ptrs_.erase(it);
    return ptr;
  }

 private:
  class Element {
   public:
    explicit Element(Ptr<Interface> ptr) : ptr_(std::move(ptr)) {
      ptr_.set_connection_error_handler(base::BindOnce(&DeleteElement, this));
    }

    ~Element() {}

    void Close() {
      ptr_.reset();

      // Resetting the interface ptr means that it won't call this object back
      // on connection error anymore, so this object must delete itself now.
      DeleteElement(this);
    }

    Interface* get() { return ptr_.get(); }

    Ptr<Interface> Take() { return std::move(ptr_); }

    base::WeakPtr<Element> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    void FlushForTesting() { ptr_.FlushForTesting(); }

   private:
    static void DeleteElement(Element* element) { delete element; }

    Ptr<Interface> ptr_;
    base::WeakPtrFactory<Element> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Element);
  };

  void ClearNullPtrs() {
    base::EraseIf(ptrs_, [](const auto& pair) { return !(pair.second); });
  }

  InterfacePtrSetElementId next_ptr_id_ = 0;
  std::map<InterfacePtrSetElementId, base::WeakPtr<Element>> ptrs_;
};

}  // namespace internal

template <typename Interface>
using InterfacePtrSet = internal::PtrSet<Interface, InterfacePtr>;

template <typename Interface>
using AssociatedInterfacePtrSet =
    internal::PtrSet<Interface, AssociatedInterfacePtr>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_PTR_SET_H_

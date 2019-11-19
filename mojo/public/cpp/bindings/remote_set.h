// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_

#include <iterator>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/util/type_safety/id_type.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace mojo {

namespace internal {
struct RemoteSetElementIdTypeTag {};
}  // namespace internal

using RemoteSetElementId = util::IdTypeU32<internal::RemoteSetElementIdTypeTag>;

// Shared implementation of a set of remotes, used by both RemoteSet and
// AssociatedRemoteSet aliases (see below).
//
// A RemoteSet or AssociatedRemoteSet is a collection of remote interface
// endpoints whose lifetime is conveniently managed by the set, i.e., remotes
// are removed from the set automatically when losing a connection).
template <typename Interface,
          template <typename>
          class RemoteType,
          template <typename>
          class PendingRemoteType>
class RemoteSetImpl {
 public:
  using Storage = std::map<RemoteSetElementId, RemoteType<Interface>>;

  // An iterator definition to support range-for iteration over RemoteSet
  // objects. An iterator can be dereferenced to get at the Remote, and |id()|
  // can be called to get the element's ID (for e.g. later removal).
  struct Iterator {
    using self_type = Iterator;
    using value_type = RemoteType<Interface>;
    using reference = const value_type&;
    using pointer = const value_type*;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

    explicit Iterator(typename Storage::const_iterator it) : it_(it) {}

    self_type& operator++() {
      ++it_;
      return *this;
    }
    self_type operator++(int) {
      self_type result(*this);
      ++(*this);
      return result;
    }
    self_type& operator--() {
      --it_;
      return *this;
    }
    self_type operator--(int) {
      self_type result(*this);
      --(*this);
      return result;
    }

    RemoteSetElementId id() const { return it_->first; }

    reference operator*() const { return it_->second; }
    pointer operator->() const { return &it_->second; }
    bool operator==(const self_type& rhs) { return it_ == rhs.it_; }
    bool operator!=(const self_type& rhs) { return it_ != rhs.it_; }

   private:
    typename Storage::const_iterator it_;
  };

  RemoteSetImpl() = default;
  ~RemoteSetImpl() = default;

  // Adds a new remote to this set and returns a unique ID that can be used to
  // identify the remote later.
  RemoteSetElementId Add(RemoteType<Interface> remote) {
    auto id = GenerateNextElementId();
    remote.set_disconnect_handler(base::BindOnce(&RemoteSetImpl::OnDisconnect,
                                                 base::Unretained(this), id));
    auto result = storage_.emplace(id, std::move(remote));
    DCHECK(result.second);
    return id;
  }

  // Same as above but for the equivalent pending remote type, for convenience.
  RemoteSetElementId Add(PendingRemoteType<Interface> remote) {
    return Add(RemoteType<Interface>(std::move(remote)));
  }

  // Removes a remote from the set given |id|, if present.
  void Remove(RemoteSetElementId id) { storage_.erase(id); }

  // Indicates whether a remote with the given ID is present in the set.
  bool Contains(RemoteSetElementId id) { return base::Contains(storage_, id); }

  // Sets a callback to invoke any time a remote in the set is disconnected.
  // Note that the remote in question is already removed from the set by the
  // time the callback is run for its disconnection.
  using DisconnectHandler = base::RepeatingCallback<void(RemoteSetElementId)>;
  void set_disconnect_handler(DisconnectHandler handler) {
    disconnect_handler_ = std::move(handler);
  }

  void Clear() { storage_.clear(); }

  bool empty() const { return storage_.empty(); }
  size_t size() const { return storage_.size(); }

  Iterator begin() { return Iterator(storage_.begin()); }
  Iterator begin() const { return Iterator(storage_.begin()); }
  Iterator end() { return Iterator(storage_.end()); }
  Iterator end() const { return Iterator(storage_.end()); }

  void FlushForTesting() {
    for (auto& it : storage_) {
        it.second.FlushForTesting();
    }
  }

 private:
  RemoteSetElementId GenerateNextElementId() {
    return RemoteSetElementId::FromUnsafeValue(next_element_id_++);
  }

  void OnDisconnect(RemoteSetElementId id) {
    Remove(id);
    if (disconnect_handler_)
      disconnect_handler_.Run(id);
  }

  uint32_t next_element_id_ = 1;
  Storage storage_;
  DisconnectHandler disconnect_handler_;

  DISALLOW_COPY_AND_ASSIGN(RemoteSetImpl);
};

template <typename Interface>
using RemoteSet = RemoteSetImpl<Interface, Remote, PendingRemote>;

template <typename Interface>
using AssociatedRemoteSet =
    RemoteSetImpl<Interface, AssociatedRemote, PendingAssociatedRemote>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_

#include <iterator>
#include <optional>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/id_type.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"

namespace mojo {

namespace internal {
struct RemoteSetElementIdTypeTag {};
}  // namespace internal

using RemoteSetElementId = base::IdTypeU32<internal::RemoteSetElementIdTypeTag>;

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
    bool operator==(const self_type& rhs) const { return it_ == rhs.it_; }
    bool operator!=(const self_type& rhs) const { return it_ != rhs.it_; }

   private:
    typename Storage::const_iterator it_;
  };

  RemoteSetImpl() = default;

  RemoteSetImpl(const RemoteSetImpl&) = delete;
  RemoteSetImpl& operator=(const RemoteSetImpl&) = delete;

  ~RemoteSetImpl() = default;

  // Adds a new remote to this set and returns a unique ID that can be used to
  // identify the remote later.
  RemoteSetElementId Add(RemoteType<Interface> remote)
    requires(!internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    return AddImpl(std::move(remote));
  }

  // Adds a new remote to this set and returns a unique ID that can be used to
  // identify the remote later.
  RemoteSetElementId Add(
      PendingRemoteType<Interface> remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(!internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    return AddImpl(std::move(remote), std::move(task_runner));
  }

  // Adds a new remote to this set if the remote is runtime enabled and returns
  // a unique ID that can be used to identify the remote later.
  std::optional<RemoteSetElementId> Add(RemoteType<Interface> remote)
    requires(internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return std::nullopt;
    }
    return AddImpl(std::move(remote));
  }

  // Adds a new remote to this set if the remote is runtime enabled and returns
  // a unique ID that can be used to identify the remote later.
  std::optional<RemoteSetElementId> Add(
      PendingRemoteType<Interface> remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr)
    requires(internal::kIsRuntimeFeatureGuarded<Interface>)
  {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return std::nullopt;
    }
    return AddImpl(std::move(remote), std::move(task_runner));
  }

  // Removes a remote from the set given |id|, if present.
  void Remove(RemoteSetElementId id) { storage_.erase(id); }
  // Similar to the method above, but also specifies a disconnect reason.
  void RemoveWithReason(RemoteSetElementId id,
                        uint32_t custom_reason_code,
                        const std::string& description) {
    auto it = storage_.find(id);
    if (it == storage_.end()) {
      return;
    }

    it->second.ResetWithReason(custom_reason_code, description);
    storage_.erase(it);
  }

  // Indicates whether a remote with the given ID is present in the set.
  bool Contains(RemoteSetElementId id) { return base::Contains(storage_, id); }

  // Returns an `Interface*` for the given ID, that can be used to issue
  // interface calls.
  Interface* Get(RemoteSetElementId id) {
    auto it = storage_.find(id);
    if (it == storage_.end())
      return nullptr;
    return it->second.get();
  }

  // Sets a callback to invoke any time a remote in the set is disconnected.
  // Note that the remote in question is already removed from the set by the
  // time the callback is run for its disconnection.
  using DisconnectHandler = base::RepeatingCallback<void(RemoteSetElementId)>;
  using DisconnectWithReasonHandler =
      base::RepeatingCallback<void(RemoteSetElementId,
                                   uint32_t /* custom_reason */,
                                   const std::string& /* description */)>;

  void set_disconnect_handler(DisconnectHandler handler) {
    disconnect_handler_ = std::move(handler);
    disconnect_with_reason_handler_.Reset();
  }

  void set_disconnect_with_reason_handler(DisconnectWithReasonHandler handler) {
    disconnect_with_reason_handler_ = std::move(handler);
    disconnect_handler_.Reset();
  }

  void Clear() { storage_.clear(); }
  void ClearWithReason(uint32_t custom_reason_code,
                       const std::string& description) {
    for (auto& [_, remote] : storage_) {
      remote.ResetWithReason(custom_reason_code, description);
    }

    Clear();
  }

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
  // Adds a new remote to this set and returns a unique ID that can be used to
  // identify the remote later.
  RemoteSetElementId AddImpl(RemoteType<Interface> remote) {
    DCHECK(remote.is_bound());
    auto id = GenerateNextElementId();
    remote.set_disconnect_with_reason_handler(base::BindOnce(
        &RemoteSetImpl::OnDisconnect, base::Unretained(this), id));
    auto result = storage_.emplace(id, std::move(remote));
    DCHECK(result.second);
    return id;
  }

  // Same as above but for the equivalent pending remote type. If |task_runner|
  // is null, the value of |base::SequencedTaskRunner::GetCurrentDefault()| at
  // the time of the |Add()| call will be used to run scheduled tasks for the
  // remote.
  RemoteSetElementId AddImpl(
      PendingRemoteType<Interface> remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr) {
    DCHECK(remote.is_valid());
    return AddImpl(
        RemoteType<Interface>(std::move(remote), std::move(task_runner)));
  }

  RemoteSetElementId GenerateNextElementId() {
    return remote_set_element_id_generator_.GenerateNextId();
  }

  void OnDisconnect(RemoteSetElementId id,
                    uint32_t custom_reason_code,
                    const std::string& description) {
    Remove(id);
    if (disconnect_handler_)
      disconnect_handler_.Run(id);
    else if (disconnect_with_reason_handler_) {
      disconnect_with_reason_handler_.Run(id, custom_reason_code, description);
    }
  }

  RemoteSetElementId::Generator remote_set_element_id_generator_;
  Storage storage_;
  DisconnectHandler disconnect_handler_;
  DisconnectWithReasonHandler disconnect_with_reason_handler_;
};

template <typename Interface>
using RemoteSet = RemoteSetImpl<Interface, Remote, PendingRemote>;

template <typename Interface>
using AssociatedRemoteSet =
    RemoteSetImpl<Interface, AssociatedRemote, PendingAssociatedRemote>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_REMOTE_SET_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_OBSERVER_WRAPPER_H_
#define SERVICES_NETWORK_OBSERVER_WRAPPER_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {

// Manages an observer interface pointer that might come from a request-specific
// mojo::PendingRemote or a shared, factory-level fallback pointer.
//
// This pattern is used because for browser-initiated requests, observer remotes
// (like DevToolsObserver, CookieAccessObserver) might be provided specifically
// for that request via `ResourceRequest::TrustedParams`. In such cases, the
// `remote` parameter passed to the constructor will be valid, and this wrapper
// will bind and use it.
//
// For other requests (e.g., subresources initiated by a renderer), the
// `TrustedParams` does not contain these specific observer remotes. Instead,
// these requests need to use a shared observer remote that was provided by the
// browser process when the URLLoaderFactory was created and is stored within
// that factory. The `fallback` parameter passed to the constructor typically
// points to the implementation associated with this shared remote.
//
// This class abstracts this selection logic.
//
// NOTE: This implementation caches the raw pointer at construction and does not
// dynamically update if the remote disconnects.
template <typename T>
class ObserverWrapper {
 public:
  // Default constructor initializes with no observer.
  ObserverWrapper() : ptr_(nullptr) {}

  // Constructor taking a PendingRemote and an optional fallback raw pointer.
  // If the remote is valid, it's bound, and the internal pointer points to it.
  // Otherwise, the internal pointer points to the fallback.
  explicit ObserverWrapper(mojo::PendingRemote<T> remote, T* fallback = nullptr)
      : remote_(std::move(remote)) {
    ptr_ = remote_.is_bound() ? remote_.get() : fallback;
  }

  // Allows checking the wrapper in boolean contexts (true if it points to
  // a observer).
  explicit operator bool() const { return !!ptr_; }

  // Returns the cached raw pointer (either remote impl or fallback).
  T* get() const { return ptr_.get(); }
  // Provides pointer-like access via the arrow operator.
  T* operator->() const {
    CHECK(get());
    return get();
  }
  // Provides pointer-like access via the dereference operator.
  T& operator*() const {
    CHECK(get());
    return *get();
  }

  ObserverWrapper(const ObserverWrapper&) = delete;
  ObserverWrapper& operator=(const ObserverWrapper&) = delete;

  // Move constructor.
  ObserverWrapper(ObserverWrapper&& other)
      : remote_(std::move(other.remote_)),
        ptr_(std::exchange(other.ptr_, nullptr)) {}
  // Move assignment operator.
  ObserverWrapper& operator=(ObserverWrapper&& other) {
    if (this == &other) {
      return *this;
    }
    remote_ = std::move(other.remote_);
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

  // Allows taking ownership of the internal `mojo::Remote`.
  mojo::Remote<T> TakeRemote() {
    ptr_ = nullptr;
    return std::move(remote_);
  }

 private:
  // Holds the Mojo remote connection, if one was provided.
  mojo::Remote<T> remote_;
  // Cached raw pointer to either the remote implementation or the fallback.
  raw_ptr<T> ptr_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_OBSERVER_WRAPPER_H_

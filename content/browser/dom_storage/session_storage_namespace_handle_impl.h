// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_HANDLE_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/session_storage_namespace_handle.h"

namespace content {
class DOMStorageContextWrapper;

class SessionStorageNamespaceHandleImpl : public SessionStorageNamespaceHandle {
 public:
  // Constructs a `SessionStorageNamespaceHandleImpl` and allocates a new ID for
  // it.
  //
  // The allows TestRenderViewHost to instantiate these.
  static scoped_refptr<SessionStorageNamespaceHandleImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context);

  // If there is an existing `SessionStorageNamespaceHandleImpl` with the given
  // ID in the `DOMStorageContextWrapper`, this will return that object.
  // Otherwise this constructs a `SessionStorageNamespaceHandleImpl` and assigns
  // `namespace_id` to it.
  static scoped_refptr<SessionStorageNamespaceHandleImpl> Create(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namespace_id);

  // Constructs a `SessionStorageNamespaceHandleImpl` with ID `namespace_id` by
  // cloning `namespace_id_to_clone`.
  // Only set `immediately` to true to cause the clone to immediately happen,
  // where there definitely will not be a `Clone()` call from the
  // `SessionStorageNamespace` mojo object.
  static scoped_refptr<SessionStorageNamespaceHandleImpl> CloneFrom(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namespace_id,
      const std::string& namespace_id_to_clone,
      bool immediately = false);

  SessionStorageNamespaceHandleImpl(const SessionStorageNamespaceHandleImpl&) =
      delete;
  SessionStorageNamespaceHandleImpl& operator=(
      const SessionStorageNamespaceHandleImpl&) = delete;

  DOMStorageContextWrapper* context() const { return context_wrapper_.get(); }

  // `SessionStorageNamespaceHandle` implementation.
  const std::string& id() override;
  void SetShouldPersist(bool should_persist) override;
  bool should_persist() override;

  scoped_refptr<SessionStorageNamespaceHandleImpl> Clone();
  bool IsFromContext(DOMStorageContextWrapper* context);

 private:
  // Creates a mojo version.
  SessionStorageNamespaceHandleImpl(
      scoped_refptr<DOMStorageContextWrapper> context,
      std::string namespace_id);

  ~SessionStorageNamespaceHandleImpl() override;

  static void DeleteSessionNamespaceFromUIThread(
      scoped_refptr<DOMStorageContextWrapper> context_wrapper,
      std::string namespace_id,
      bool should_persist);

  scoped_refptr<DOMStorageContextWrapper> context_wrapper_;
  std::string namespace_id_;
  bool should_persist_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_NAMESPACE_HANDLE_IMPL_H_

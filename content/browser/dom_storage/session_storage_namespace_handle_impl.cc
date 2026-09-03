// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_handle_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
scoped_refptr<SessionStorageNamespaceHandleImpl>
SessionStorageNamespaceHandleImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context) {
  return SessionStorageNamespaceHandleImpl::Create(
      std::move(context), blink::AllocateSessionStorageNamespaceId());
}

// static
scoped_refptr<SessionStorageNamespaceHandleImpl>
SessionStorageNamespaceHandleImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id) {
  scoped_refptr<SessionStorageNamespaceHandleImpl> existing =
      context->MaybeGetExistingNamespace(namespace_id);
  if (existing) {
    return existing;
  }
  auto result = base::WrapRefCounted(
      new SessionStorageNamespaceHandleImpl(context, std::move(namespace_id)));
  result->context_wrapper_->GetSessionStorageControl()->CreateNamespace(
      result->namespace_id_);
  return result;
}

// static
scoped_refptr<SessionStorageNamespaceHandleImpl>
SessionStorageNamespaceHandleImpl::CloneFrom(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id,
    const std::string& namespace_id_to_clone,
    bool immediately) {
  auto result = base::WrapRefCounted(
      new SessionStorageNamespaceHandleImpl(context, std::move(namespace_id)));
  result->context_wrapper_->GetSessionStorageControl()->CloneNamespace(
      namespace_id_to_clone, result->namespace_id_,
      immediately
          ? storage::mojom::SessionStorageCloneType::kImmediate
          : storage::mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
  return result;
}

const std::string& SessionStorageNamespaceHandleImpl::id() {
  return namespace_id_;
}

void SessionStorageNamespaceHandleImpl::SetShouldPersist(bool should_persist) {
  should_persist_ = should_persist;
}

bool SessionStorageNamespaceHandleImpl::should_persist() {
  return should_persist_;
}

scoped_refptr<SessionStorageNamespaceHandleImpl>
SessionStorageNamespaceHandleImpl::Clone() {
  return CloneFrom(context_wrapper_, blink::AllocateSessionStorageNamespaceId(),
                   namespace_id_, true);
}

bool SessionStorageNamespaceHandleImpl::IsFromContext(
    DOMStorageContextWrapper* context) {
  return context_wrapper_.get() == context;
}

SessionStorageNamespaceHandleImpl::SessionStorageNamespaceHandleImpl(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id)
    : context_wrapper_(std::move(context)),
      namespace_id_(std::move(namespace_id)),
      should_persist_(false) {
  context_wrapper_->AddNamespace(namespace_id_, this);
}

SessionStorageNamespaceHandleImpl::~SessionStorageNamespaceHandleImpl() {
  context_wrapper_->RemoveNamespace(namespace_id_);
  // We must hop the the UI thread, as the context_wrapper_ can only be
  // accessed on that thread.
  base::ScopedClosureRunner deleteNamespaceRunner = base::ScopedClosureRunner(
      base::BindOnce(&SessionStorageNamespaceHandleImpl::
                         DeleteSessionNamespaceFromUIThread,
                     std::move(context_wrapper_), std::move(namespace_id_),
                     should_persist_));
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // If this fails to post then that's fine, as the mojo state should
    // already be destructed.
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        deleteNamespaceRunner.Release());
  }
}

// static
void SessionStorageNamespaceHandleImpl::DeleteSessionNamespaceFromUIThread(
    scoped_refptr<DOMStorageContextWrapper> context_wrapper,
    std::string namespace_id,
    bool should_persist) {
  storage::mojom::SessionStorageControl* session_storage =
      context_wrapper->GetSessionStorageControl();
  if (session_storage) {
    session_storage->DeleteNamespace(namespace_id, should_persist);
  }
}

}  // namespace content

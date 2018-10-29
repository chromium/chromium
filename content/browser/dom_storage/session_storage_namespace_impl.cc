// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/features.h"

namespace content {

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context) {
  return SessionStorageNamespaceImpl::Create(
      std::move(context), blink::AllocateSessionStorageNamespaceId());
}

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id) {
  scoped_refptr<SessionStorageNamespaceImpl> existing =
      context->MaybeGetExistingNamespace(namespace_id);
  if (existing)
    return existing;
  if (context->mojo_session_state()) {
    DCHECK(base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage));
    auto result = base::WrapRefCounted(
        new SessionStorageNamespaceImpl(context, std::move(namespace_id)));
    result->mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SessionStorageContextMojo::CreateSessionNamespace,
                       base::Unretained(context->mojo_session_state()),
                       result->namespace_id_));
    return result;
  }
  scoped_refptr<DOMStorageContextImpl> context_impl = context->context();
  context_impl->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace,
                                context_impl, namespace_id));
  return base::WrapRefCounted(new SessionStorageNamespaceImpl(
      std::move(context), std::move(context_impl), std::move(namespace_id)));
}

// static
scoped_refptr<SessionStorageNamespaceImpl>
SessionStorageNamespaceImpl::CloneFrom(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id,
    const std::string& namespace_id_to_clone,
    bool immediately) {
  if (context->mojo_session_state()) {
    DCHECK(base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage));
    auto result = base::WrapRefCounted(
        new SessionStorageNamespaceImpl(context, std::move(namespace_id)));
    result->mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SessionStorageContextMojo::CloneSessionNamespace,
                       base::Unretained(context->mojo_session_state()),
                       namespace_id_to_clone, result->namespace_id_,
                       immediately
                           ? SessionStorageContextMojo::CloneType::kImmediate
                           : SessionStorageContextMojo::CloneType::
                                 kWaitForCloneOnNamespace));
    return result;
  }
  scoped_refptr<DOMStorageContextImpl> context_impl = context->context();
  context_impl->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DOMStorageContextImpl::CloneSessionNamespace,
                     context_impl, namespace_id_to_clone, namespace_id));
  return base::WrapRefCounted(new SessionStorageNamespaceImpl(
      std::move(context), std::move(context_impl), std::move(namespace_id)));
}

const std::string& SessionStorageNamespaceImpl::id() const {
  return namespace_id_;
}

void SessionStorageNamespaceImpl::SetShouldPersist(bool should_persist) {
  should_persist_ = should_persist;
}

bool SessionStorageNamespaceImpl::should_persist() const {
  return should_persist_;
}

scoped_refptr<SessionStorageNamespaceImpl>
SessionStorageNamespaceImpl::Clone() {
  return CloneFrom(context_wrapper_, blink::AllocateSessionStorageNamespaceId(),
                   namespace_id_, true);
}

bool SessionStorageNamespaceImpl::IsFromContext(
    DOMStorageContextWrapper* context) {
  return context_wrapper_.get() == context;
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    scoped_refptr<DOMStorageContextWrapper> context_wrapper,
    scoped_refptr<DOMStorageContextImpl> context,
    std::string namespace_id)
    : context_(std::move(context)),
      context_wrapper_(std::move(context_wrapper)),
      namespace_id_(std::move(namespace_id)),
      should_persist_(false) {
  context_wrapper_->AddNamespace(namespace_id_, this);
  DCHECK(!base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage));
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id)
    : context_wrapper_(std::move(context)),
      mojo_task_runner_(context_wrapper_->mojo_task_runner()),
      namespace_id_(std::move(namespace_id)),
      should_persist_(false) {
  context_wrapper_->AddNamespace(namespace_id_, this);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kOnionSoupDOMStorage));
}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
  DCHECK(context_ || mojo_task_runner_);
  context_wrapper_->RemoveNamespace(namespace_id_);
  if (context_) {
    context_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageContextImpl::DeleteSessionNamespace, context_,
                       namespace_id_, should_persist_));
  }
  if (IsMojoSessionStorage()) {
    base::ScopedClosureRunner deleteNamespaceRunner =
        base::ScopedClosureRunner(base::BindOnce(
            &SessionStorageNamespaceImpl::DeleteSessionNamespaceFromUIThread,
            std::move(mojo_task_runner_), std::move(context_wrapper_),
            std::move(namespace_id_), should_persist_));
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      // If this fails to post then that's fine, as the mojo state should
      // already be destructed.
      base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                               deleteNamespaceRunner.Release());
    }
  }
}

// static
void SessionStorageNamespaceImpl::DeleteSessionNamespaceFromUIThread(
    scoped_refptr<base::SequencedTaskRunner> mojo_task_runner,
    scoped_refptr<DOMStorageContextWrapper> context_wrapper,
    std::string namespace_id,
    bool should_persist) {
  if (context_wrapper->mojo_session_state()) {
    mojo_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&SessionStorageContextMojo::DeleteSessionNamespace,
                       base::Unretained(context_wrapper->mojo_session_state()),
                       namespace_id, should_persist));
  }
}

}  // namespace content

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/keep_alive_handle_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace content {

class KeepAliveHandleFactory::Context final : public base::RefCounted<Context> {
 public:
  explicit Context(int process_id) : process_id_(process_id) {
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled())
      return;
    process_host->IncrementKeepAliveRefCount();
  }

  void Detach() {
    if (detached_)
      return;
    detached_ = true;
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled())
      return;

    process_host->DecrementKeepAliveRefCount();
  }

  void DetachLater(base::TimeDelta timeout) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                          base::BindOnce(&Context::Detach, AsWeakPtr()),
                          timeout);
  }

  void AddReceiver(std::unique_ptr<mojom::KeepAliveHandle> impl,
                   mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) {
    receiver_set_.Add(std::move(impl), std::move(receiver));
  }

  base::WeakPtr<Context> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  friend class base::RefCounted<Context>;
  ~Context() { Detach(); }

  mojo::UniqueReceiverSet<mojom::KeepAliveHandle> receiver_set_;
  const int process_id_;
  bool detached_ = false;

  base::WeakPtrFactory<Context> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Context);
};

class KeepAliveHandleFactory::KeepAliveHandleImpl final
    : public mojom::KeepAliveHandle {
 public:
  explicit KeepAliveHandleImpl(scoped_refptr<Context> context)
      : context_(std::move(context)) {}
  ~KeepAliveHandleImpl() override {}

 private:
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveHandleImpl);
};

KeepAliveHandleFactory::KeepAliveHandleFactory(RenderProcessHost* process_host)
    : process_id_(process_host->GetID()) {}

KeepAliveHandleFactory::~KeepAliveHandleFactory() {
  if (context_)
    context_->DetachLater(timeout_);
}

void KeepAliveHandleFactory::Create(
    mojo::PendingReceiver<mojom::KeepAliveHandle> receiver) {
  scoped_refptr<Context> context;
  if (context_) {
    context = context_.get();
  } else {
    context = base::MakeRefCounted<Context>(process_id_);
    context_ = context->AsWeakPtr();
  }

  context->AddReceiver(std::make_unique<KeepAliveHandleImpl>(context),
                       std::move(receiver));
}

void KeepAliveHandleFactory::SetTimeout(base::TimeDelta timeout) {
  timeout_ = timeout;
}

}  // namespace content

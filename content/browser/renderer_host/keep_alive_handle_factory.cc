// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/keep_alive_handle_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"

namespace content {

class KeepAliveHandleFactory::KeepAliveHandleImpl final
    : public blink::mojom::KeepAliveHandle {
 public:
  explicit KeepAliveHandleImpl(int process_id) : process_id_(process_id) {
    GetContentClient()->browser()->OnKeepaliveRequestStarted();
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled()) {
      return;
    }
    process_host->IncrementKeepAliveRefCount(
        RenderProcessHost::KeepAliveSource::KEEP_ALIVE_HANDLE_FACTORY);
  }
  ~KeepAliveHandleImpl() override {
    GetContentClient()->browser()->OnKeepaliveRequestFinished();
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled()) {
      return;
    }
    process_host->DecrementKeepAliveRefCount();
  }

  KeepAliveHandleImpl(const KeepAliveHandleImpl&) = delete;
  KeepAliveHandleImpl& operator=(const KeepAliveHandleImpl&) = delete;

 private:
  const int process_id_;
};

class KeepAliveHandleFactory::Context final
    : public base::RefCounted<Context>,
      public blink::mojom::KeepAliveHandleFactory {
 public:
  explicit Context(int process_id) : process_id_(process_id) {}

  void Clear() {
    handle_receivers_.Clear();
    factory_receivers_.Clear();
  }

  void IssueKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandle> receiver) override {
    handle_receivers_.Add(std::make_unique<KeepAliveHandleImpl>(process_id_),
                          std::move(receiver));
  }

  void Bind(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
    factory_receivers_.Add(this, std::move(receiver));
  }

  void ClearLater(base::TimeDelta timeout) {
    GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(&Context::Clear, this), timeout);
  }

 private:
  friend class base::RefCounted<Context>;
  ~Context() override = default;

  mojo::UniqueReceiverSet<blink::mojom::KeepAliveHandle> handle_receivers_;
  mojo::ReceiverSet<blink::mojom::KeepAliveHandleFactory> factory_receivers_;
  const int process_id_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

KeepAliveHandleFactory::KeepAliveHandleFactory(RenderProcessHost* process_host,
                                               base::TimeDelta timeout)
    : context_(base::MakeRefCounted<Context>(process_host->GetID())),
      timeout_(timeout) {}

KeepAliveHandleFactory::~KeepAliveHandleFactory() {
  context_->ClearLater(timeout_);
}

void KeepAliveHandleFactory::Bind(
    mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
  context_->Bind(std::move(receiver));
}

}  // namespace content

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

namespace {

class KeepAliveHandleImpl final : public blink::mojom::KeepAliveHandle {
 public:
  explicit KeepAliveHandleImpl(int process_id) : process_id_(process_id) {
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    GetContentClient()->browser()->OnKeepaliveRequestStarted(
        process_host ? process_host->GetBrowserContext() : nullptr);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled()) {
      return;
    }
    process_host->IncrementKeepAliveRefCount();
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

}  // namespace

class KeepAliveHandleFactory::Context
    : public blink::mojom::KeepAliveHandleFactory {
 public:
  explicit Context(int process_id) : process_id_(process_id) {}
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  ~Context() override = default;

  void IssueKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandle> receiver) override {
    handle_receivers_.Add(std::make_unique<KeepAliveHandleImpl>(process_id_),
                          std::move(receiver));
  }

  void Bind(
      mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
    factory_receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::UniqueReceiverSet<blink::mojom::KeepAliveHandle> handle_receivers_;
  mojo::ReceiverSet<blink::mojom::KeepAliveHandleFactory> factory_receivers_;
  const int process_id_;
};

KeepAliveHandleFactory::KeepAliveHandleFactory(RenderProcessHost* process_host,
                                               base::TimeDelta timeout)
    : context_(std::make_unique<Context>(process_host->GetID())),
      timeout_(timeout) {}

KeepAliveHandleFactory::~KeepAliveHandleFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Extend the lifetime of `context_` a bit. Note that `context_` has an
  // ability to extend the lifetime of the associated render process.
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<Context>) {}, std::move(context_)),
      timeout_);
}

void KeepAliveHandleFactory::Bind(
    mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
  context_->Bind(std::move(receiver));
}

}  // namespace content

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/keep_alive_handle_factory.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle.mojom.h"
#include "third_party/blink/public/mojom/loader/keep_alive_handle_factory.mojom.h"

namespace content {

namespace {

std::atomic_uint64_t handle_sequence_id{0};

inline uint64_t GetNextHandleId() {
  return handle_sequence_id.fetch_add(1, std::memory_order_relaxed);
}

class KeepAliveHandleImpl final : public blink::mojom::KeepAliveHandle {
 public:
  explicit KeepAliveHandleImpl(int process_id)
      : process_id_(process_id), handle_id_(GetNextHandleId()) {
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    GetContentClient()->browser()->OnKeepaliveRequestStarted(
        process_host ? process_host->GetBrowserContext() : nullptr);
    if (!process_host || process_host->AreRefCountsDisabled()) {
      return;
    }
    static_cast<RenderProcessHostImpl*>(process_host)
        ->IncrementKeepAliveRefCount(handle_id_);
  }
  ~KeepAliveHandleImpl() override {
    GetContentClient()->browser()->OnKeepaliveRequestFinished();
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->AreRefCountsDisabled()) {
      return;
    }
    static_cast<RenderProcessHostImpl*>(process_host)
        ->DecrementKeepAliveRefCount(handle_id_);
  }

  KeepAliveHandleImpl(const KeepAliveHandleImpl&) = delete;
  KeepAliveHandleImpl& operator=(const KeepAliveHandleImpl&) = delete;

 private:
  const int process_id_;
  // A unique identifier for this KeepAliveHandle that can be recorded in
  // Increment/DecrementKeepAliveRefCount for debugging purposes.
  // TODO(wjmaclean): Once we understand the root causes of
  // https://crbug.com/1148542, we can remove this.
  uint64_t handle_id_;
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
      FROM_HERE, base::DoNothingWithBoundArgs(std::move(context_)), timeout_);
}

void KeepAliveHandleFactory::Bind(
    mojo::PendingReceiver<blink::mojom::KeepAliveHandleFactory> receiver) {
  context_->Bind(std::move(receiver));
}

}  // namespace content

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource_cache_impl.h"

#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

namespace {

mojom::blink::ResourceCacheContainsResult::LifecycleState
ConvertFrameLifecycleState(mojom::blink::FrameLifecycleState state) {
  switch (state) {
    case mojom::blink::FrameLifecycleState::kRunning:
      return mojom::blink::ResourceCacheContainsResult::LifecycleState::
          kRunning;
    case mojom::blink::FrameLifecycleState::kPaused:
      return mojom::blink::ResourceCacheContainsResult::LifecycleState::kPaused;
    case mojom::blink::FrameLifecycleState::kFrozen:
    case mojom::blink::FrameLifecycleState::kFrozenAutoResumeMedia:
      return mojom::blink::ResourceCacheContainsResult::LifecycleState::kFrozen;
  }
}

}  // namespace

// static
void ResourceCacheImpl::Bind(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::ResourceCache> receiver) {
  DCHECK(frame);
  frame->BindResourceCache(std::move(receiver));
}

ResourceCacheImpl::ResourceCacheImpl(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::ResourceCache> receiver)
    : frame_(frame),
      receivers_(this, frame->GetDocument()->GetExecutionContext()) {
  DCHECK(frame_);
  receivers_.Add(std::move(receiver), frame->GetDocument()->GetTaskRunner(
                                          TaskType::kNetworkingUnfreezable));
}

void ResourceCacheImpl::AddReceiver(
    mojo::PendingReceiver<mojom::blink::ResourceCache> receiver) {
  receivers_.Add(std::move(receiver), frame_->GetDocument()->GetTaskRunner(
                                          TaskType::kNetworkingUnfreezable));
}

void ResourceCacheImpl::ClearReceivers() {
  receivers_.Clear();
}

void ResourceCacheImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(receivers_);
}

void ResourceCacheImpl::Contains(const KURL& url, ContainsCallback callback) {
  Document* document = frame_->GetDocument();
  DCHECK(document);
  auto result = mojom::blink::ResourceCacheContainsResult::New();
  result->ipc_response_time = base::TimeTicks::Now();

  ExecutionContext* context = document->GetExecutionContext();
  if (!context) {
    std::move(callback).Run(std::move(result));
    return;
  }

  result->is_in_cache = MemoryCache::Get()->ResourceForURL(url) != nullptr;
  result->is_visible = document->IsPageVisible();
  result->lifecycle_state =
      ConvertFrameLifecycleState(context->ContextPauseState());
  std::move(callback).Run(std::move(result));
}

}  // namespace blink

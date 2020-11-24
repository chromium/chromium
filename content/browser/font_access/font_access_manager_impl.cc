// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/task/post_task.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"

namespace content {

FontAccessManagerImpl::FontAccessManagerImpl() {
  ipc_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  results_task_runner_ = content::GetUIThreadTaskRunner({});
}

FontAccessManagerImpl::~FontAccessManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FontAccessManagerImpl::BindReceiver(
    const BindingContext& context,
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver), context);
}

void FontAccessManagerImpl::EnumerateLocalFonts(
    EnumerateLocalFontsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  const BindingContext& context = receivers_.current_context();

  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(context.frame_id);
  if (rfh == nullptr) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kUnexpectedError,
        base::ReadOnlySharedMemoryRegion());
    return;
  }

  // Page Visibility is required for the API to function at all.
  if (rfh->visibility() == blink::mojom::FrameVisibility::kNotRendered) {
    std::move(callback).Run(blink::mojom::FontEnumerationStatus::kNotVisible,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  auto status = PermissionControllerImpl::FromBrowserContext(
                    rfh->GetProcess()->GetBrowserContext())
                    ->GetPermissionStatusForFrame(PermissionType::FONT_ACCESS,
                                                  rfh, context.origin.GetURL());

  if (status != blink::mojom::PermissionStatus::ASK) {
    // Permission has been requested before.
    DidRequestPermission(std::move(callback), std::move(status));
    return;
  }

  // Transient User Activation only required before showing permission prompt.
  // This action will consume it.
  if (!rfh->HasTransientUserActivation()) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kNeedsUserActivation,
        base::ReadOnlySharedMemoryRegion());
    return;
  }
  rfh->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
      blink::mojom::UserActivationNotificationType::kNone);

  PermissionControllerImpl::FromBrowserContext(
      rfh->GetProcess()->GetBrowserContext())
      ->RequestPermission(
          PermissionType::FONT_ACCESS, rfh, context.origin.GetURL(),
          /*user_gesture=*/true,
          base::BindOnce(&FontAccessManagerImpl::DidRequestPermission,
                         // Safe because this is an initialized singleton.
                         base::Unretained(this), std::move(callback)));
#else
  std::move(callback).Run(blink::mojom::FontEnumerationStatus::kUnimplemented,
                          base::ReadOnlySharedMemoryRegion());
#endif
}

void FontAccessManagerImpl::DidRequestPermission(
    EnumerateLocalFontsCallback callback,
    blink::mojom::PermissionStatus status) {
  if (status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kPermissionDenied,
        base::ReadOnlySharedMemoryRegion());
    return;
  }

// Per-platform delegation for obtaining cached font enumeration data occurs
// here, after the permission has been granted.
#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](EnumerateLocalFontsCallback callback,
                        scoped_refptr<base::TaskRunner> results_task_runner) {
                       FontEnumerationCache::GetInstance()
                           ->QueueShareMemoryRegionWhenReady(
                               results_task_runner, std::move(callback));
                     },
                     std::move(callback), results_task_runner_));
#else
  std::move(callback).Run(blink::mojom::FontEnumerationStatus::kUnimplemented,
                          base::ReadOnlySharedMemoryRegion());
#endif
}

}  // namespace content

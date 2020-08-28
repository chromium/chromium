// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_WIN)
#include "content/browser/font_access/font_enumeration_cache_win.h"
#endif

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
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver), context);
}

#if defined(OS_MAC)
void FontAccessManagerImpl::RequestPermission(
    RequestPermissionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const BindingContext& context = receivers_.current_context();
  RenderFrameHost* rfh = RenderFrameHost::FromID(context.frame_id);

  // Double checking: renderer processes should already have checked for user
  // activation before the RPC has been made. It is not an error, because it is
  // possible that user activation has lapsed before reaching here.
  if (!rfh->HasTransientUserActivation()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  PermissionControllerImpl::FromBrowserContext(
      rfh->GetProcess()->GetBrowserContext())
      ->RequestPermission(PermissionType::FONT_ACCESS, rfh,
                          context.origin.GetURL(),
                          /*user_gesture=*/true,
                          base::BindOnce(
                              [](RequestPermissionCallback callback,
                                 blink::mojom::PermissionStatus status) {
                                std::move(callback).Run(status);
                              },
                              std::move(callback)));
}
#endif

void FontAccessManagerImpl::EnumerateLocalFonts(
    EnumerateLocalFontsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(OS_WIN)
  const BindingContext& context = receivers_.current_context();
  RenderFrameHost* rfh = RenderFrameHost::FromID(context.frame_id);

  // Double checking: renderer processes should already have checked for user
  // activation before the RPC has been made. It is not an error, because it is
  // possible that user activation has lapsed before reaching here.
  if (!rfh->HasTransientUserActivation()) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kPermissionDenied,
        base::ReadOnlySharedMemoryRegion());
    return;
  }

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
#if defined(OS_WIN)
  ipc_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](EnumerateLocalFontsCallback callback,
                        scoped_refptr<base::TaskRunner> results_task_runner) {
                       FontEnumerationCacheWin::GetInstance()
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

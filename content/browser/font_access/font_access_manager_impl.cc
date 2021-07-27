// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/font_access_delegate.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace content {

// static
std::unique_ptr<FontAccessManagerImpl> FontAccessManagerImpl::Create() {
  return std::make_unique<FontAccessManagerImpl>(
      FontEnumerationCache::Create(), base::PassKey<FontAccessManagerImpl>());
}

// static
std::unique_ptr<FontAccessManagerImpl> FontAccessManagerImpl::CreateForTesting(
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache) {
  return std::make_unique<FontAccessManagerImpl>(
      std::move(font_enumeration_cache),
      base::PassKey<FontAccessManagerImpl>());
}

FontAccessManagerImpl::FontAccessManagerImpl(
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache,
    base::PassKey<FontAccessManagerImpl>)
    : font_enumeration_cache_(std::move(font_enumeration_cache)),
      results_task_runner_(content::GetUIThreadTaskRunner({})) {}

FontAccessManagerImpl::~FontAccessManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FontAccessManagerImpl::BindReceiver(
    url::Origin origin,
    GlobalRenderFrameHostId frame_id,
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver),
                 {
                     .origin = std::move(origin),
                     .frame_id = frame_id,
                 });
}

void FontAccessManagerImpl::EnumerateLocalFonts(
    EnumerateLocalFontsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  if (skip_privacy_checks_for_testing_) {
    DidRequestPermission(std::move(callback),
                         blink::mojom::PermissionStatus::GRANTED);
    return;
  }

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

void FontAccessManagerImpl::ChooseLocalFonts(
    const std::vector<std::string>& selection,
    ChooseLocalFontsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  std::move(callback).Run(blink::mojom::FontEnumerationStatus::kUnimplemented,
                          {});
#else
  const BindingContext& context = receivers_.current_context();

  RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromID(context.frame_id);
  if (rfh == nullptr) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kUnexpectedError, {});
    return;
  }

  // Page Visibility is required for the API to function at all.
  if (rfh->visibility() == blink::mojom::FrameVisibility::kNotRendered) {
    std::move(callback).Run(blink::mojom::FontEnumerationStatus::kNotVisible,
                            {});
    return;
  }

  // Transient User Activation required before showing the chooser.
  // This action will consume it.
  if (!rfh->HasTransientUserActivation()) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kNeedsUserActivation, {});
    return;
  }
  rfh->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kConsumeTransientActivation,
      blink::mojom::UserActivationNotificationType::kNone);

  FontAccessDelegate* delegate =
      GetContentClient()->browser()->GetFontAccessDelegate();
  choosers_[context.frame_id] = delegate->RunChooser(
      rfh, selection,
      base::BindOnce(&FontAccessManagerImpl::DidChooseLocalFonts,
                     base::Unretained(this), context.frame_id,
                     std::move(callback)));
#endif
}

void FontAccessManagerImpl::FindAllFonts(FindAllFontsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  std::move(callback).Run(blink::mojom::FontEnumerationStatus::kUnimplemented,
                          {});
#else
  // TODO(pwnall): base::Unretained is unsafe. Introduce a factory.
  font_enumeration_cache_
      .AsyncCall(&FontEnumerationCache::QueueShareMemoryRegionWhenReady)
      .WithArgs(results_task_runner_,
                base::BindOnce(&FontAccessManagerImpl::DidFindAllFonts,
                               base::Unretained(this), std::move(callback)));
#endif
}

void FontAccessManagerImpl::DidRequestPermission(
    EnumerateLocalFontsCallback callback,
    blink::mojom::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if !defined(PLATFORM_HAS_LOCAL_FONT_ENUMERATION_IMPL)
  std::move(callback).Run(blink::mojom::FontEnumerationStatus::kUnimplemented,
                          base::ReadOnlySharedMemoryRegion());
  return;
#else
  if (status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kPermissionDenied,
        base::ReadOnlySharedMemoryRegion());
    return;
  }

// Per-platform delegation for obtaining cached font enumeration data occurs
// here, after the permission has been granted.
  font_enumeration_cache_
      .AsyncCall(&FontEnumerationCache::QueueShareMemoryRegionWhenReady)
      .WithArgs(results_task_runner_, std::move(callback));
#endif
}

void FontAccessManagerImpl::DidFindAllFonts(
    FindAllFontsCallback callback,
    blink::mojom::FontEnumerationStatus status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::mojom::FontEnumerationStatus::kOk) {
    std::move(callback).Run(status, {});
    return;
  }

  const base::ReadOnlySharedMemoryMapping mapping = region.Map();
  if (mapping.size() > INT_MAX) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kUnexpectedError, {});
    return;
  }

  blink::FontEnumerationTable table;
  table.ParseFromArray(mapping.memory(), static_cast<int>(mapping.size()));

  std::vector<blink::mojom::FontMetadata> data;
  for (const auto& element : table.fonts()) {
    auto entry = blink::mojom::FontMetadata(
        element.postscript_name(), element.full_name(), element.family(),
        element.style(), element.italic(), element.stretch(), element.weight());
    data.push_back(std::move(entry));
  }

  std::move(callback).Run(status, std::move(data));
}

void FontAccessManagerImpl::DidChooseLocalFonts(
    GlobalRenderFrameHostId frame_id,
    ChooseLocalFontsCallback callback,
    blink::mojom::FontEnumerationStatus status,
    std::vector<blink::mojom::FontMetadataPtr> fonts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The chooser has fulfilled its purpose. It's safe to dispose of it.
  size_t erased = choosers_.erase(frame_id);
  DCHECK_EQ(erased, 1u);

  std::move(callback).Run(std::move(status), std::move(fonts));
}

}  // namespace content

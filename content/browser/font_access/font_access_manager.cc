// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_access/font_access_manager.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "content/browser/font_access/font_enumeration_cache.h"
#include "content/browser/font_access/font_enumeration_data_source.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/font_access/font_enumeration_table.pb.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace content {

// static
std::unique_ptr<FontAccessManager> FontAccessManager::Create() {
  return std::make_unique<FontAccessManager>(
      FontEnumerationCache::Create(), base::PassKey<FontAccessManager>());
}

// static
std::unique_ptr<FontAccessManager> FontAccessManager::CreateForTesting(
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache) {
  return std::make_unique<FontAccessManager>(
      std::move(font_enumeration_cache), base::PassKey<FontAccessManager>());
}

FontAccessManager::FontAccessManager(
    base::SequenceBound<FontEnumerationCache> font_enumeration_cache,
    base::PassKey<FontAccessManager>)
    : font_enumeration_cache_(std::move(font_enumeration_cache)),
      results_task_runner_(GetUIThreadTaskRunner({})) {}

FontAccessManager::~FontAccessManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FontAccessManager::BindReceiver(
    GlobalRenderFrameHostId frame_id,
    mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver),
                 {
                     .frame_id = frame_id,
                 });
}

void FontAccessManager::EnumerateLocalFonts(
    EnumerateLocalFontsCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kFontAccess));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
  if (rfh->GetVisibilityState() !=
      blink::mojom::PageVisibilityState::kVisible) {
    std::move(callback).Run(blink::mojom::FontEnumerationStatus::kNotVisible,
                            base::ReadOnlySharedMemoryRegion());
    return;
  }

  content::PermissionController* permission_controller =
      rfh->GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  auto status = permission_controller->GetPermissionStatusForCurrentDocument(
      blink::PermissionType::LOCAL_FONTS, rfh);

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

  permission_controller->RequestPermissionFromCurrentDocument(
      rfh,
      PermissionRequestDescription(blink::PermissionType::LOCAL_FONTS,
                                   /*user_gesture=*/true),
      base::BindOnce(&FontAccessManager::DidRequestPermission,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FontAccessManager::DidRequestPermission(
    EnumerateLocalFontsCallback callback,
    blink::mojom::PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        blink::mojom::FontEnumerationStatus::kPermissionDenied,
        base::ReadOnlySharedMemoryRegion());
    return;
  }

  // Per-platform delegation for obtaining cached font enumeration data occurs
  // here, after the permission has been granted.
  font_enumeration_cache_
      .AsyncCall(&FontEnumerationCache::GetFontEnumerationData)
      .Then(base::BindOnce(
          [](EnumerateLocalFontsCallback callback, FontEnumerationData data) {
            std::move(callback).Run(data.status, std::move(data.font_data));
          },
          std::move(callback)));
}

}  // namespace content

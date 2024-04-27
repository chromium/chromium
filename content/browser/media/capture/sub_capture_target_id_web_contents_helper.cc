// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/sub_capture_target_id_web_contents_helper.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#error Region Capture not supported on Android.
#endif

namespace content {

// TODO(crbug.com/40203554): Remove this protected static function.
// See header for more details.
base::Token SubCaptureTargetIdWebContentsHelper::GUIDToToken(
    const base::Uuid& guid) {
  std::string lowercase = guid.AsLowercaseString();

  // |lowercase| is either empty, or follows the expected pattern.
  // TODO(crbug.com/40201847): Resolve open question of correct treatment
  // of an invalid GUID.
  if (lowercase.empty()) {
    return base::Token();
  }
  DCHECK_EQ(lowercase.length(), 32u + 4u);  // 32 hex-chars; 4 hyphens.

  base::RemoveChars(lowercase, "-", &lowercase);
  DCHECK_EQ(lowercase.length(), 32u);  // 32 hex-chars; 0 hyphens.

  std::string_view string_piece(lowercase);

  uint64_t high = 0;
  bool success = base::HexStringToUInt64(string_piece.substr(0, 16), &high);
  DCHECK(success);

  uint64_t low = 0;
  success = base::HexStringToUInt64(string_piece.substr(16, 16), &low);
  DCHECK(success);

  return base::Token(high, low);
}

WebContents* SubCaptureTargetIdWebContentsHelper::GetRelevantWebContents(
    GlobalRenderFrameHostId rfh_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40287690): Remove this redundant check.
  if (rfh_id == GlobalRenderFrameHostId()) {
    return nullptr;
  }

  RenderFrameHostImpl* rfhi = RenderFrameHostImpl::FromID(rfh_id);
  if (!rfhi || !rfhi->IsActive()) {
    return nullptr;
  }
  rfhi = rfhi->GetMainFrame();  // TODO(crbug.com/40287690): Remove this line.

  if (GetContentClient()
          ->browser()
          ->UseOutermostMainFrameOrEmbedderForSubCaptureTargets()) {
    rfhi = rfhi->GetOutermostMainFrameOrEmbedder();
  }

  return WebContents::FromRenderFrameHost(rfhi);
}

SubCaptureTargetIdWebContentsHelper::SubCaptureTargetIdWebContentsHelper(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<SubCaptureTargetIdWebContentsHelper>(*web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);
}

SubCaptureTargetIdWebContentsHelper::~SubCaptureTargetIdWebContentsHelper() =
    default;

std::string SubCaptureTargetIdWebContentsHelper::ProduceId(Type type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(type == Type::kCropTarget || type == Type::kRestrictionTarget);

  std::vector<base::Token>& ids =
      (type == Type::kCropTarget) ? crop_ids_ : restriction_ids_;

  // Prevent Web-applications from producing an excessive number of IDs.
  if (ids.size() >= kMaxIdsPerWebContents) {
    return std::string();
  }

  // Given the exceedingly low likelihood of collisions, the check for
  // uniqueness could have theoretically been skipped. But it's cheap
  // enough to perform, given that `kMaxIdsPerWebContents` is so small
  // compared to the space of GUIDs, and it guarantees we never silently fail
  // the application by cropping/restricting to the wrong, duplicate target.
  base::Uuid guid;
  base::Token id;
  do {
    guid = base::Uuid::GenerateRandomV4();
    id = GUIDToToken(guid);
  } while (IsAssociatedWith(id, type));
  ids.push_back(id);

  return guid.AsLowercaseString();
}

bool SubCaptureTargetIdWebContentsHelper::IsAssociatedWith(
    const base::Token& id,
    Type type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(type == Type::kCropTarget || type == Type::kRestrictionTarget);

  const std::vector<base::Token>& ids =
      (type == Type::kCropTarget) ? crop_ids_ : restriction_ids_;

  return base::Contains(ids, id);
}

void SubCaptureTargetIdWebContentsHelper::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(navigation_handle);

  // Cross-document navigation of the top-level frame invalidates all IDs
  // associated with the observed WebContents.
  // Using IsInPrimaryMainFrame is valid here since the browser only caches this
  // state for the active main frame.
  if (!navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    ClearIds();
  }
}

void SubCaptureTargetIdWebContentsHelper::ClearIds() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  crop_ids_.clear();
  restriction_ids_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SubCaptureTargetIdWebContentsHelper);

}  // namespace content

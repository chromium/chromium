// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/crop_id_web_contents_helper.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/callback.h"
#include "base/token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#error Region Capture not supported on Android.
#endif

namespace content {

// TODO(crbug.com/1264849): Remove this protected static function.
// See header for more details.
base::Token CropIdWebContentsHelper::GUIDToToken(const base::Uuid& guid) {
  std::string lowercase = guid.AsLowercaseString();

  // |lowercase| is either empty, or follows the expected pattern.
  // TODO(crbug.com/1260380): Resolve open question of correct treatment
  // of an invalid GUID.
  if (lowercase.empty()) {
    return base::Token();
  }
  DCHECK_EQ(lowercase.length(), 32u + 4u);  // 32 hex-chars; 4 hyphens.

  base::RemoveChars(lowercase, "-", &lowercase);
  DCHECK_EQ(lowercase.length(), 32u);  // 32 hex-chars; 0 hyphens.

  base::StringPiece string_piece(lowercase);

  uint64_t high = 0;
  bool success = base::HexStringToUInt64(string_piece.substr(0, 16), &high);
  DCHECK(success);

  uint64_t low = 0;
  success = base::HexStringToUInt64(string_piece.substr(16, 16), &low);
  DCHECK(success);

  return base::Token(high, low);
}

CropIdWebContentsHelper::CropIdWebContentsHelper(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      WebContentsUserData<CropIdWebContentsHelper>(*web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents);
}

CropIdWebContentsHelper::~CropIdWebContentsHelper() = default;

std::string CropIdWebContentsHelper::ProduceCropId() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Prevent Web-applications from producing an excessive number of crop-IDs.
  if (crop_ids_.size() >= kMaxCropIdsPerWebContents) {
    return std::string();
  }

  // Given the exceedingly low likelihood of collisions, the check for
  // uniqueness could have theoretically been skipped. But it's cheap
  // enough to perform, given that `kMaxCropIdsPerWebContents` is so small
  // compared to the space of GUIDs, and it guarantees we never silently fail
  // the application by cropping to the wrong, duplicate target.
  base::Uuid guid;
  base::Token crop_id;
  do {
    guid = base::Uuid::GenerateRandomV4();
    crop_id = GUIDToToken(guid);
  } while (IsAssociatedWithCropId(crop_id));
  crop_ids_.push_back(crop_id);

  return guid.AsLowercaseString();
}

bool CropIdWebContentsHelper::IsAssociatedWithCropId(
    const base::Token& crop_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return base::Contains(crop_ids_, crop_id);
}

void CropIdWebContentsHelper::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(navigation_handle);

  // Cross-document navigation of the top-level frame invalidates all crop-IDs
  // associated with the observed WebContents.
  // Using IsInPrimaryMainFrame is valid here since the browser only caches this
  // state for the active main frame.
  if (!navigation_handle->IsSameDocument() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    ClearCropIds();
  }
}

void CropIdWebContentsHelper::ClearCropIds() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  crop_ids_.clear();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CropIdWebContentsHelper);

}  // namespace content

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CROP_ID_WEB_CONTENTS_HELPER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CROP_ID_WEB_CONTENTS_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/guid.h"
#include "base/token.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

#if BUILDFLAG(IS_ANDROID)
#error Region Capture not supported on Android.
#endif

namespace content {

class NavigationHandle;

class CONTENT_EXPORT CropIdWebContentsHelper final
    : public WebContentsObserver,
      public WebContentsUserData<CropIdWebContentsHelper> {
 public:
  // Limits the number of crop-IDs a given Web-application can produce
  // so as to limit the potential for abuse.
  // Known and accepted issue - embedded iframes can be intentionally disruptive
  // by producing too many crop-IDs. It's up to the Web-application to not
  // embed such iframes.
  constexpr static size_t kMaxCropIdsPerWebContents = 100;

  explicit CropIdWebContentsHelper(WebContents* web_contents);
  CropIdWebContentsHelper(const CropIdWebContentsHelper&) = delete;
  CropIdWebContentsHelper& operator=(const CropIdWebContentsHelper&) = delete;
  ~CropIdWebContentsHelper() final;

  // Produces a new crop-ID, records its association with this WebContents
  // and returns it.
  // This method can soft-fail if invoked more than |kMaxCropIdsPerWebContents|
  // times for a given WebContents. Failure is signaled by returning an
  // empty string.
  std::string ProduceCropId();

  // Checks whether this WebContents is associated with a crop-ID.
  // This allows us to check whether a call to cropTo() by the Web-application
  // is permitted.
  bool IsAssociatedWithCropId(const base::Token& crop_id) const;

 protected:
  // TODO(crbug.com/1264849): Remove this local copy of GUIDToToken().
  // It is copy of a function that is not currently visible from the browser
  // process. It should be made visible to the browser process and reused
  // rather than redefined. It is defined as protected so that unit tests
  // can use it, too.
  static base::Token GUIDToToken(const base::GUID& guid);

 private:
  friend class WebContentsUserData<CropIdWebContentsHelper>;
  friend class CropIdWebContentsHelperTest;

  // WebContentsObserver implementation.
  // Cross-document navigation of the top-level document discards all crop-IDs
  // associated with the top-level WebContents.
  // TODO(crbug.com/1264849): Record per RFH and treat its navigation.
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) final;

  // Forgets all associations of crop-IDs to this WebContents.
  // TODO(crbug.com/1264849): Clear per-RFH or throughout.
  void ClearCropIds();

  // Records which crop-IDs are associated with this WebContents.
  // At most |kMaxCropIdsPerWebContents|, as discussed where
  // |kMaxCropIdsPerWebContents| is defined.
  std::vector<base::Token> crop_ids_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CROP_ID_WEB_CONTENTS_HELPER_H_

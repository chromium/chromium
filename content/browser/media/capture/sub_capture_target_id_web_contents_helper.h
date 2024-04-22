// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_SUB_CAPTURE_TARGET_ID_WEB_CONTENTS_HELPER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_SUB_CAPTURE_TARGET_ID_WEB_CONTENTS_HELPER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/token.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "media/capture/mojom/video_capture_types.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#error Region Capture not supported on Android.
#endif

namespace content {

class NavigationHandle;

class CONTENT_EXPORT SubCaptureTargetIdWebContentsHelper final
    : public WebContentsObserver,
      public WebContentsUserData<SubCaptureTargetIdWebContentsHelper> {
 public:
  using Type = media::mojom::SubCaptureTargetType;

  // Limits the number of SubCaptureTargetIds a given Web-application can
  // produce of a given type, so as to limit the potential for abuse.
  // Known and accepted issue - embedded iframes can be intentionally disruptive
  // by producing too many IDs. It's up to the Web-application to not
  // embed such iframes.
  constexpr static size_t kMaxIdsPerWebContents = 100;

  // Returns the WebContents associated with a given GlobalRenderFrameHostId.
  //
  // This would normally be the WebContents for that very RFH.
  // But if the relevant setting is set in ContentBrowserClient,
  // this function returns the WebContents for the *outermost* main frame
  // or embedder, traversing the parent tree across <iframe> and <webview>
  // boundaries.
  //
  // If no such WebContents is found, nullptr is returned.
  static WebContents* GetRelevantWebContents(GlobalRenderFrameHostId rfh_id);

  explicit SubCaptureTargetIdWebContentsHelper(WebContents* web_contents);
  SubCaptureTargetIdWebContentsHelper(
      const SubCaptureTargetIdWebContentsHelper&) = delete;
  SubCaptureTargetIdWebContentsHelper& operator=(
      const SubCaptureTargetIdWebContentsHelper&) = delete;
  ~SubCaptureTargetIdWebContentsHelper() final;

  // Produces a new SubCaptureTargetId, records its association with
  // this WebContents and returns it.
  // This method can soft-fail if invoked more than |kMaxIdsPerWebContents|
  // times for a given WebContents.
  // Failure is signaled by returning an empty string.
  std::string ProduceId(Type type);

  // Checks whether this WebContents is associated with a SubCaptureTargetId.
  // This allows us to check whether a call to cropTo() or restrictTo()
  // by the Web-application is permitted.
  bool IsAssociatedWith(const base::Token& id, Type type) const;

 protected:
  // TODO(crbug.com/40203554): Remove this local copy of GUIDToToken().
  // It is copy of a function that is not currently visible from the browser
  // process. It should be made visible to the browser process and reused
  // rather than redefined. It is defined as protected so that unit tests
  // can use it, too.
  static base::Token GUIDToToken(const base::Uuid& guid);

 private:
  friend class WebContentsUserData<SubCaptureTargetIdWebContentsHelper>;
  friend class SubCaptureTargetIdWebContentsHelperTest;

  // WebContentsObserver implementation.
  // Cross-document navigation of the top-level document discards all
  // SubCaptureTargetIds associated with the top-level WebContents.
  // TODO(crbug.com/40203554): Record per RFH and treat its navigation.
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) final;

  // Forgets all associations of SubCaptureTargetIds to this WebContents.
  // TODO(crbug.com/40203554): Clear per-RFH or throughout.
  void ClearIds();

  // Records which SubCaptureTargetIds are associated with this WebContents.
  // At most |kMaxIdsPerWebContents| of each type, as discussed where
  // |kMaxIdsPerWebContents| is defined.
  std::vector<base::Token> crop_ids_;
  std::vector<base::Token> restriction_ids_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_SUB_CAPTURE_TARGET_ID_WEB_CONTENTS_HELPER_H_

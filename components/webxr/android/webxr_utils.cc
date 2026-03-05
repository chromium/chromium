// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webxr/android/webxr_utils.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace webxr {

content::WebContents* GetWebContents(
    const content::GlobalRenderFrameHostId& frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(frame_id);
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  return web_contents;
}

base::android::ScopedJavaLocalRef<jobject> GetJavaWebContents(
    const content::GlobalRenderFrameHostId& frame_id) {
  return GetWebContents(frame_id)->GetJavaWebContents();
}

}  // namespace webxr

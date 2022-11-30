// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_handle.h"

#include <utility>

#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

WebContents* NavigationHandle::GetWebContents() {
  // The NavigationRequest cannot access the WebContentsImpl as it would be a
  // layering violation, hence the cast here.
  return static_cast<WebContentsImpl*>(
      NavigationRequest::From(this)->GetDelegate());
}

}  // namespace content

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

class GURL;

namespace content {
class RenderFrameHost;

class PermissionUtil {
 public:
  // Returns the authoritative `embedding origin`, as a GURL, to be used for
  // permission decisions in `render_frame_host`.
  // TODO(crbug.com/1327384): Remove this method when possible.
  static GURL GetLastCommittedOriginAsURL(
      content::RenderFrameHost* render_frame_host);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

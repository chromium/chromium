// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

bool PermissionControllerDelegate::IsPermissionOverridable(
    blink::PermissionType permission,
    const absl::optional<url::Origin>& origin) {
  return true;
}

PermissionResult
PermissionControllerDelegate::GetPermissionResultForCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host) {
  return PermissionResult(blink::mojom::PermissionStatus::DENIED,
                          PermissionStatusSource::UNSPECIFIED);
}

absl::optional<gfx::Rect>
PermissionControllerDelegate::GetExclusionAreaBoundsInScreen(
    content::WebContents* web_contents) const {
  return absl::nullopt;
}

}  // namespace content

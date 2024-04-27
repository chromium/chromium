// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {
class RenderFrameHost;

class PermissionUtil {
 public:
  // Returns the authoritative `embedding origin`, as a GURL, to be used for
  // permission decisions in `render_frame_host`.
  // TODO(crbug.com/40226169): Remove this method when possible.
  CONTENT_EXPORT static GURL GetLastCommittedOriginAsURL(
      content::RenderFrameHost* render_frame_host);

  // Determines whether the passed-in descriptor indicates a domain override is
  // being used. The override mechanism is currently only used by one permission
  // type, specifically storage access requests on behalf of another domain.
  CONTENT_EXPORT static bool IsDomainOverride(
      const blink::mojom::PermissionDescriptorPtr& descriptor);

  // For a descriptor that indicates a domain override is used, retrieve it as
  // an origin. The override mechanism is currently only used by one permission
  // type, specifically storage access requests on behalf of another domain.
  CONTENT_EXPORT static const url::Origin& ExtractDomainOverride(
      const blink::mojom::PermissionDescriptorPtr& descriptor);

  // Determine whether the domain override mechanism is enabled by features. The
  // override mechanism is currently only used by one permission type,
  // specifically storage access requests on behalf of another domain.
  CONTENT_EXPORT static bool IsDomainOverrideEnabled();

  // For a domain override, determines whether it is valid. The override
  // mechanism is currently only used by one permission type, specifically
  // storage access requests on behalf of another domain.
  CONTENT_EXPORT static bool ValidateDomainOverride(
      const std::vector<blink::PermissionType>& types,
      RenderFrameHost* rfh,
      const blink::mojom::PermissionDescriptorPtr& descriptor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

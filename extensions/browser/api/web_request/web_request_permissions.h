// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "content/public/common/resource_type.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/origin.h"

class GURL;

namespace extensions {
class PermissionHelper;
struct WebRequestInfo;
}

// This class is used to test whether extensions may modify web requests. It
// should be used on the IO thread.
class WebRequestPermissions {
 public:
  // Different host permission checking modes for CanExtensionAccessURL.
  enum HostPermissionsCheck {
    DO_NOT_CHECK_HOST = 0,  // No check.
    // Permission needed for given request URL.
    // TODO(karandeepb): Remove this checking mode.
    REQUIRE_HOST_PERMISSION_FOR_URL,
    // Same as REQUIRE_HOST_PERMISSION_FOR_URL but sub-resource requests will
    // also need access to the request initiator.
    REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
    // Permission needed for <all_urls>.
    REQUIRE_ALL_URLS
  };

  // Returns true if the request shall not be reported to extensions.
  static bool HideRequest(extensions::PermissionHelper* permission_helper,
                          const extensions::WebRequestInfo& request);

  // Helper function used only in tests, sets a variable which enables or
  // disables a CHECK.
  static void AllowAllExtensionLocationsInPublicSessionForTesting(bool value);

  // |host_permission_check| controls how permissions are checked with regard to
  // |url| and |initiator| if an initiator exists.
  static extensions::PermissionsData::PageAccess CanExtensionAccessURL(
      extensions::PermissionHelper* permission_helper,
      const std::string& extension_id,
      const GURL& url,
      int tab_id,
      bool crosses_incognito,
      HostPermissionsCheck host_permissions_check,
      const base::Optional<url::Origin>& initiator,
      content::ResourceType resource_type);

  static bool CanExtensionAccessInitiator(
      extensions::PermissionHelper* permission_helper,
      const extensions::ExtensionId extension_id,
      const base::Optional<url::Origin>& initiator,
      int tab_id,
      bool crosses_incognito);

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRequestPermissions);
};

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_

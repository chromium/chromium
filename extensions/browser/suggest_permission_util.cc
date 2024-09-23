// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/suggest_permission_util.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/permissions_info.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace extensions {

namespace {

const char kPermissionsHelpURLForExtensions[] =
    "https://developer.chrome.com/extensions/manifest.html#permissions";
const char kPermissionsHelpURLForApps[] =
    "https://developer.chrome.com/apps/declare_permissions.html";

void SuggestAPIPermissionInDevToolsConsole(
    mojom::APIPermissionID permission,
    const Extension* extension,
    content::RenderFrameHost* render_frame_host) {
  const APIPermissionInfo* permission_info =
      PermissionsInfo::GetInstance()->GetByID(permission);
  CHECK(permission_info);

  // Note, intentionally not internationalizing this string, as it is output
  // as a log message to developers in the developer tools console.
  std::string message = base::StringPrintf(
      "Is the '%s' permission appropriate? See %s.",
      permission_info->name(),
      extension->is_platform_app() ?
          kPermissionsHelpURLForApps : kPermissionsHelpURLForExtensions);

  render_frame_host->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kWarning, message);
}

}  // namespace

bool IsExtensionWithPermissionOrSuggestInConsole(
    mojom::APIPermissionID permission,
    const Extension* extension,
    content::RenderFrameHost* render_frame_host) {
  if (extension &&
      extension->permissions_data()->HasAPIPermission(permission)) {
    return true;
  }

  if (extension && render_frame_host) {
    SuggestAPIPermissionInDevToolsConsole(permission, extension,
                                          render_frame_host);
  }

  return false;
}

}  // namespace extensions

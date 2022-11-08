// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/extension_action/action_info_test_util.h"

#include "extensions/common/manifest_constants.h"

namespace extensions {

const char* GetAPINameForActionType(ActionInfo::Type action_type) {
  const char* api_name = nullptr;
  switch (action_type) {
    case ActionInfo::TYPE_BROWSER:
      api_name = "browserAction";
      break;
    case ActionInfo::TYPE_PAGE:
      api_name = "pageAction";
      break;
    case ActionInfo::TYPE_ACTION:
      api_name = "action";
      break;
  }

  return api_name;
}

const ActionInfo* GetActionInfoOfType(const Extension& extension,
                                      ActionInfo::Type type) {
  const ActionInfo* action_info =
      ActionInfo::GetExtensionActionInfo(&extension);
  return (action_info && action_info->type == type) ? action_info : nullptr;
}

int GetManifestVersionForActionType(ActionInfo::Type type) {
  int manifest_version = 0;
  switch (type) {
    case ActionInfo::TYPE_BROWSER:
    case ActionInfo::TYPE_PAGE:
      manifest_version = 2;
      break;
    case ActionInfo::TYPE_ACTION:
      manifest_version = 3;
      break;
  }

  return manifest_version;
}

}  // namespace extensions

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_URL_LOADING_TYPES_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_URL_LOADING_TYPES_H_

#include <string>

#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/source.h"

namespace web_app {

struct GeneratedResponse {
  std::string response_body;
};

using IwaSourceWithModeOrGeneratedResponse =
    std::variant<IwaSourceWithMode, GeneratedResponse>;

}  // namespace web_app

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_TYPES_URL_LOADING_TYPES_H_

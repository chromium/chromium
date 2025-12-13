// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SCHEME_H_
#define COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SCHEME_H_

#include <stddef.h>

namespace webapps {

// The isolated-app: scheme is used for Isolated Web Apps. A public explainer
// can be found here: https://github.com/reillyeon/isolated-web-apps
inline constexpr char kIsolatedAppScheme[] = "isolated-app";
inline constexpr char16_t kIsolatedAppSchemeUtf16[] = u"isolated-app";

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_ISOLATED_WEB_APPS_SCHEME_H_

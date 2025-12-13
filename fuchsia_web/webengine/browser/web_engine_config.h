// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_

#include "base/values.h"
#include "fuchsia_web/webengine/web_engine_export.h"

class GURL;

namespace base {
class CommandLine;
}  // namespace base

// Updates the `command_line` based on `config`. Returns `false` if the config
// is invalid.
WEB_ENGINE_EXPORT bool UpdateCommandLineFromConfigFile(
    const base::Value::Dict& config,
    base::CommandLine* command_line);

// Returns if a service worker should be persistent even when the resources are
// limited.
WEB_ENGINE_EXPORT bool IsProtectedServiceWorker(const GURL& scope);

// Returns if a document or service worker of |origin| is allowed using
// Notification permission.
// On WebEngine, the platform notification is not supported, i.e. the
// notification won't show up to the end users. But if Notification permission
// is granted, WebEngine treats the display of the notifications as succeeded.
WEB_ENGINE_EXPORT bool AllowNotifications(const GURL& origin);

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_WEB_ENGINE_CONFIG_H_

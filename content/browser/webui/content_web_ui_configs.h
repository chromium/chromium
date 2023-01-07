// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONFIGS_H_
#define CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONFIGS_H_

#include "content/common/content_export.h"

namespace content {

// Method that adds /content's `WebUIConfig`s to `WebUIConfigMap`.
void CONTENT_EXPORT RegisterContentWebUIConfigs();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_CONTENT_WEB_UI_CONFIGS_H_

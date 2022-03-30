// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/favicon_util.h"

namespace extensions::favicon_util {

void GetFaviconForExtensionRequest(const Extension* extension,
                                   std::string* mime_type,
                                   std::string* charset,
                                   std::string* data) {
  // TODO(solomonkinard): Check extension permission.
  *mime_type = "text/plain";
  *charset = "utf-8";
  *data = "Favicon";
  // TODO(solomonkinard): Return actual favicon.
}

}  // namespace extensions::favicon_util

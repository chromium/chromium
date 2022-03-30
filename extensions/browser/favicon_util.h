// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_FAVICON_UTIL_H_
#define EXTENSIONS_BROWSER_FAVICON_UTIL_H_

#include <string>

namespace extensions {

class Extension;

namespace favicon_util {

void GetFaviconForExtensionRequest(const Extension* extension,
                                   std::string* mime_type,
                                   std::string* charset,
                                   std::string* data);

}  // namespace favicon_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_FAVICON_UTIL_H_

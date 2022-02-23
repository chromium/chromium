// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_DEVICE_LOCAL_ACCOUNT_UTIL_H_
#define EXTENSIONS_BROWSER_DEVICE_LOCAL_ACCOUNT_UTIL_H_

#include <string>

// This file contains utilities used for device local accounts (public sessions
// / kiosks). Eg. check whether an extension is allowlisted for use in public
// session.

namespace extensions {

bool IsAllowlistedForPublicSession(const std::string& extension_id);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_DEVICE_LOCAL_ACCOUNT_UTIL_H_

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_ID_H_
#define EXTENSIONS_COMMON_EXTENSION_ID_H_

#include <set>
#include <string>
#include <vector>

namespace extensions {

// If valid, uniquely identifies an Extension using 32 characters from the
// alphabet 'a'-'p'.
using ExtensionId = std::string;

using ExtensionIdList = std::vector<ExtensionId>;
using ExtensionIdSet = std::set<ExtensionId>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_ID_H_

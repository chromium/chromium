// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_TYPES_H_
#define EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_TYPES_H_

#include <vector>

namespace extensions {

namespace api::clipboard {
struct AdditionalDataItem;
}

using AdditionalDataItemList = std::vector<api::clipboard::AdditionalDataItem>;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CLIPBOARD_CLIPBOARD_API_TYPES_H_

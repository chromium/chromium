// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_
#define EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_

namespace extensions {

// The Safe Browsing extension allowlist states.
//
// Note that not allowlisted extensions are only disabled if the allowlist
// enforcement is enabled.
enum AllowlistState {
  // The allowlist state is unknown.
  ALLOWLIST_UNDEFINED = 0,
  // The extension is included in the Safe Browsing extension allowlist.
  ALLOWLIST_ALLOWLISTED = 1,
  // The extension is not included in the Safe Browsing extension allowlist.
  ALLOWLIST_NOT_ALLOWLISTED = 2,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ALLOWLIST_STATE_H_

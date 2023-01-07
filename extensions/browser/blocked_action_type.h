// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOCKED_ACTION_TYPE_H_
#define EXTENSIONS_BROWSER_BLOCKED_ACTION_TYPE_H_

namespace extensions {

// Types of actions that an extension can perform that can be blocked (typically
// while waiting for user action).
enum BlockedActionType {
  BLOCKED_ACTION_NONE = 0,
  BLOCKED_ACTION_SCRIPT_AT_START = 1 << 0,
  BLOCKED_ACTION_SCRIPT_OTHER = 1 << 1,
  BLOCKED_ACTION_WEB_REQUEST = 1 << 2,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOCKED_ACTION_TYPE_H_

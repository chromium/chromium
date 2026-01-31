// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_BACK_FORWARD_NAVIGATION_TYPE_H_
#define IOS_WEB_NAVIGATION_BACK_FORWARD_NAVIGATION_TYPE_H_

namespace web {

// Defines the type of navigation in the Back Forward navigation list.
enum class BackForwardNavigationType {
  kBackward,
  kForward,
  kToEntry,
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_BACK_FORWARD_NAVIGATION_TYPE_H_

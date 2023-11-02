// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_GLOBAL_STATE_UTIL_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_GLOBAL_STATE_UTIL_H_

namespace ios_web_view {

// Initializes global ios_web_view state. Classes reling on the web state being
// initialized should call |InitializeGlobalState| first. It is ok if this
// method is called more than once. All calls to |InitializeGlobalState| after
// the state has already been initialized are no-ops.
void InitializeGlobalState();

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_GLOBAL_STATE_UTIL_H_

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_DEFAULT_APPS_UTIL_H_
#define REMOTING_HOST_WIN_DEFAULT_APPS_UTIL_H_

namespace remoting {

// Launches the Windows 'settings' modern app with the 'default apps' view
// focused. This only works for Windows 8 and Windows 10.
// Returns a boolean indicating whether the default apps view is successfully
// launched.
bool LaunchDefaultAppsSettingsModernDialog();

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_DEFAULT_APPS_UTIL_H_

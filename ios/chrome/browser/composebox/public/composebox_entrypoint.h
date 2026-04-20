// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ENTRYPOINT_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ENTRYPOINT_H_

/// Enum defining invocation points for the composebox.
enum class ComposeboxEntrypoint {
  /// The AIM button on NTP.
  kNTPAIMButton,
  /// The fakebox on NTP.
  kNTPFakebox,
  /// The plus button on NTP.
  kNTPPlusButton,
  /// The cobrowse experience.
  kCobrowse,
  /// Other, commands from OmniboxCommand.
  kOther,
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ENTRYPOINT_H_

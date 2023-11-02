// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TYPES_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TYPES_H_

namespace extensions {

struct DownloadPingData {
  // The number of days it's been since our last rollcall or active ping,
  // respectively. These are calculated based on the start of day from the
  // server's perspective.
  int rollcall_days = 0;
  int active_days = 0;

  // Whether the extension is enabled or not.
  bool is_enabled = true;

  // A bitmask of disable_reason::DisableReason's, which may contain one or
  // more reasons why an extension is disabled.
  int disable_reasons = 0;

  constexpr DownloadPingData() = default;
  constexpr DownloadPingData(int rollcall,
                             int active,
                             bool enabled,
                             int reasons)
      : rollcall_days(rollcall),
        active_days(active),
        is_enabled(enabled),
        disable_reasons(reasons) {}
};

// The priority of the download request.
enum class DownloadFetchPriority {
  // Used for update requests not initiated by a user, for example regular
  // extension updates started by the scheduler.
  kBackground,

  // Used for on-demate update requests i.e. requests initiated by a users.
  kForeground,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_DOWNLOADER_TYPES_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_downloader_types.h"

#include "extensions/browser/disable_reason.h"

namespace extensions {

DownloadPingData::DownloadPingData() = default;

DownloadPingData::DownloadPingData(int rollcall,
                                   int active,
                                   bool enabled,
                                   const DisableReasonSet& disable_reasons)
    : rollcall_days(rollcall),
      active_days(active),
      is_enabled(enabled),
      disable_reasons(disable_reasons) {}

DownloadPingData::~DownloadPingData() = default;

}  // namespace extensions

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_USAGE_STATS_CONSENT_H_
#define REMOTING_HOST_USAGE_STATS_CONSENT_H_

namespace remoting {

// Retrieves the user's consent to collect crash dumps and gather usage
// statistics.
bool GetUsageStatsConsent(bool* allowed, bool* set_by_policy);

// Retrieves the effective user's consent to collect crash dumps and gather
// usage statistics. In most cases the returned value matches |allowed| returned
// by GetUsageStatsConsent(). If GetUsageStatsConsent() fails this routine
// reports that crash dump repoting is disabled.
bool IsUsageStatsAllowed();

// Records the user's consent to collect crash dumps and gather usage
// statistics.
bool SetUsageStatsConsent(bool allowed);

}  // namespace remoting

#endif  // REMOTING_HOST_USAGE_STATS_CONSENT_H_

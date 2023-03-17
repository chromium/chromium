// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web_state_list/session_metrics.h"

#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

namespace {
// Global whose address is used as a unique key to find the
// SessionMetrics associated to a particular ChromeBrowserState.
const int kSessionMetricsKey = 0;
}  // namespace

bool operator&(MetricsToRecordFlagSet set, MetricsToRecordFlags flag) {
  return static_cast<unsigned int>(set) & static_cast<unsigned int>(flag);
}

MetricsToRecordFlagSet operator|(MetricsToRecordFlagSet set,
                                 MetricsToRecordFlags flag) {
  return static_cast<MetricsToRecordFlagSet>(static_cast<unsigned int>(set) |
                                             static_cast<unsigned int>(flag));
}

MetricsToRecordFlagSet operator|(MetricsToRecordFlags lhs,
                                 MetricsToRecordFlags rhs) {
  return static_cast<MetricsToRecordFlagSet>(static_cast<unsigned int>(lhs) |
                                             static_cast<unsigned int>(rhs));
}

SessionMetrics::SessionMetrics() = default;

SessionMetrics::~SessionMetrics() = default;

// static
SessionMetrics* SessionMetrics::FromBrowserState(
    ChromeBrowserState* browser_state) {
  SessionMetrics* session_metrics = static_cast<SessionMetrics*>(
      browser_state->GetUserData(&kSessionMetricsKey));

  if (!session_metrics) {
    browser_state->SetUserData(&kSessionMetricsKey,
                               std::make_unique<SessionMetrics>());
    session_metrics = static_cast<SessionMetrics*>(
        browser_state->GetUserData(&kSessionMetricsKey));
  }

  DCHECK(session_metrics);
  return session_metrics;
}

void SessionMetrics::RecordAndClearSessionMetrics(
    MetricsToRecordFlagSet flag_set) {
  if (flag_set & MetricsToRecordFlags::kActivatedTabCount) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Session.OpenedTabCounts",
                                activated_web_state_counter_, 1, 200, 50);
  }

  ResetSessionMetrics();
}

void SessionMetrics::RecordAndClearSessionMetrics(MetricsToRecordFlags flag) {
  RecordAndClearSessionMetrics(static_cast<MetricsToRecordFlagSet>(flag));
}

void SessionMetrics::OnWebStateActivated() {
  activated_web_state_counter_++;
}

void SessionMetrics::ResetSessionMetrics() {
  activated_web_state_counter_ = 0;
}

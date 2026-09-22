// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PERSISTENT_STATE_H_
#define IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PERSISTENT_STATE_H_

#import <string>

#import "base/time/time.h"
#import "base/version.h"

@class NSUserDefaults;

// Represents the state of the OmahaService that is persisted between
// application execution.
struct OmahaPersistentState {
  // The time at which the next ping should be sent.
  base::Time next_ping_time;

  // The time at which the last ping was sent.
  base::Time last_ping_time;

  // The time at which the last response from Omaha server was received.
  base::Time last_response_time;

  // The last version for which an installation ping has been sent.
  base::Version last_sent_version;

  // Identifier of the request in flight. Empty if there are no request.
  std::string current_request_id;

  // Number of failures since the last successful ping.
  int number_of_failures = 0;

  // Last received server date.
  int last_server_date = 0;

  // Load persistent state from NSUserDefaults.
  static OmahaPersistentState LoadFrom(NSUserDefaults* defaults);

  // Save persistent state into NSUserDefaults.
  static void SaveTo(NSUserDefaults* defaults,
                     const OmahaPersistentState& state);

  friend constexpr bool operator==(const OmahaPersistentState& lhs,
                                   const OmahaPersistentState& rhs) = default;
};

#endif  // IOS_CHROME_BROWSER_OMAHA_MODEL_OMAHA_PERSISTENT_STATE_H_

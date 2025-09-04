// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_SESSION_JSON_UTILS_H_
#define NET_DEVICE_BOUND_SESSIONS_SESSION_JSON_UTILS_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_params.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

// Utilities for parsing the JSON session specification
// https://github.com/WICG/dbsc/blob/main/README.md#session-registration-instructions-json

// Parse the full JSON as a string. Returns:
// - A `SessionParams` describing the session to be created on success
// - A `SessionError` on all failures. If the JSON contains "continue":
//   false, we will return a `kServerRequestedTermination` error, and
//   `kInvalidSessionConfig` in other cases.
base::expected<SessionParams, SessionError> ParseSessionInstructionJson(
    GURL fetcher_url,
    unexportable_keys::UnexportableKeyId key_id,
    std::optional<std::string> expected_session_id,
    std::string_view response_json);

std::optional<WellKnownParams> ParseWellKnownJson(
    std::string_view response_json);

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_SESSION_JSON_UTILS_H_

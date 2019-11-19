// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/common/api/declarative_net_request.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {

// Returns true if |data| represents a valid data buffer containing indexed
// ruleset data with |expected_checksum|.
bool IsValidRulesetData(base::span<const uint8_t> data, int expected_checksum);

// Returns the version header used for indexed ruleset files. Only exposed for
// testing.
std::string GetVersionHeaderForTesting();

// Returns the indexed ruleset format version.
int GetIndexedRulesetFormatVersionForTesting();

// Override the ruleset format version for testing.
void SetIndexedRulesetFormatVersionForTesting(int version);

// Strips the version header from |ruleset_data|. Returns false on version
// mismatch.
bool StripVersionHeaderAndParseVersion(std::string* ruleset_data);

// Helper function to persist the indexed ruleset |data| at the given |path|.
// The ruleset is composed of a version header corresponding to the current
// ruleset format version, followed by the actual ruleset data. Note: The
// checksum only corresponds to this ruleset data and does not include the
// version header.
bool PersistIndexedRuleset(const base::FilePath& path,
                           base::span<const uint8_t> data,
                           int* ruleset_checksum);

// Helper to clear each renderer's in-memory cache the next time it navigates.
void ClearRendererCacheOnNavigation();

// Helper to log the |kReadDynamicRulesJSONStatusHistogram| histogram.
void LogReadDynamicRulesStatus(ReadJSONRulesResult::Status status);

// Constructs an api::declarative_net_request::RequestDetails from a
// WebRequestInfo.
api::declarative_net_request::RequestDetails CreateRequestDetails(
    const WebRequestInfo& request);

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_UTILS_H_

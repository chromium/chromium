// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TEST_NET_LOG_UTIL_H_
#define NET_LOG_TEST_NET_LOG_UTIL_H_

#include <stddef.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "net/log/net_log_event_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

struct NetLogEntry;

// Checks that the element of |entries| at |offset| has the provided values.
// A negative |offset| indicates a position relative to the end of |entries|.
// Checks to make sure |offset| is within bounds, and fails gracefully if it
// isn't.
::testing::AssertionResult LogContainsEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event,
    NetLogEventPhase expected_phase);

// Just like LogContainsEvent, but always checks for an EventPhase of
// PHASE_BEGIN.
::testing::AssertionResult LogContainsBeginEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event);

// Just like LogContainsEvent, but always checks for an EventPhase of PHASE_END.
::testing::AssertionResult LogContainsEndEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event);

// Just like LogContainsEvent, but does not check phase.
::testing::AssertionResult LogContainsEntryWithType(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType type);

// Check if the log contains an entry of the given type at |start_offset| or
// after.  It is not a failure if there's an earlier matching entry.  Negative
// offsets are relative to the end of the array.
::testing::AssertionResult LogContainsEntryWithTypeAfter(
    const std::vector<NetLogEntry>& entries,
    int start_offset,
    NetLogEventType type);

// Check if the first entry with the specified values is at |start_offset| or
// after. It is a failure if there's an earlier matching entry.  Negative
// offsets are relative to the end of the array.
size_t ExpectLogContainsSomewhere(const std::vector<NetLogEntry>& entries,
                                  size_t min_offset,
                                  NetLogEventType expected_event,
                                  NetLogEventPhase expected_phase);

// Check if the log contains an entry with  the given values at |start_offset|
// or after.  It is not a failure if there's an earlier matching entry.
// Negative offsets are relative to the end of the array.
size_t ExpectLogContainsSomewhereAfter(const std::vector<NetLogEntry>& entries,
                                       size_t start_offset,
                                       NetLogEventType expected_event,
                                       NetLogEventPhase expected_phase);

// The following methods return a parameter of the given type at the given path,
// or nullopt if there is none.
std::optional<std::string> GetOptionalStringValueFromParams(
    const NetLogEntry& entry,
    std::string_view path);
std::optional<bool> GetOptionalBooleanValueFromParams(const NetLogEntry& entry,
                                                      std::string_view path);
std::optional<int> GetOptionalIntegerValueFromParams(const NetLogEntry& entry,
                                                     std::string_view path);
std::optional<int> GetOptionalNetErrorCodeFromParams(const NetLogEntry& entry);

// Same as the *Optional* versions above, except will add a Gtest failure if the
// value was not present, and then return some default.
std::string GetStringValueFromParams(const NetLogEntry& entry,
                                     std::string_view path);
int GetIntegerValueFromParams(const NetLogEntry& entry, std::string_view path);
bool GetBooleanValueFromParams(const NetLogEntry& entry, std::string_view path);
int GetNetErrorCodeFromParams(const NetLogEntry& entry);

}  // namespace net

#endif  // NET_LOG_TEST_NET_LOG_UTIL_H_

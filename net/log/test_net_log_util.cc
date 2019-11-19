// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log_util.h"

#include <cstddef>

#include "net/log/net_log.h"

namespace net {

namespace {

// Takes the list of entries and an offset, and returns an index into the array.
// If |offset| is positive, just returns |offset|.  If it's negative, it
// indicates a position relative to the end of the array.
size_t GetIndex(const std::vector<NetLogEntry>& entries, int offset) {
  if (offset >= 0)
    return static_cast<size_t>(offset);

  size_t abs_offset = static_cast<size_t>(-offset);
  // If offset indicates a position before the start of the array, just return
  // the end of the list.
  if (abs_offset > entries.size())
    return entries.size();
  return entries.size() - abs_offset;
}

}  // namespace

::testing::AssertionResult LogContainsEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event,
    NetLogEventPhase expected_phase) {
  size_t index = GetIndex(entries, offset);
  if (index >= entries.size())
    return ::testing::AssertionFailure() << index << " is out of bounds.";
  const NetLogEntry& entry = entries[index];
  if (expected_event != entry.type) {
    return ::testing::AssertionFailure()
           << "Actual event: " << NetLog::EventTypeToString(entry.type)
           << ". Expected event: " << NetLog::EventTypeToString(expected_event)
           << ".";
  }
  if (expected_phase != entry.phase) {
    return ::testing::AssertionFailure()
           << "Actual phase: " << static_cast<int>(entry.phase)
           << ". Expected phase: " << static_cast<int>(expected_phase) << ".";
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult LogContainsBeginEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event) {
  return LogContainsEvent(entries, offset, expected_event,
                          NetLogEventPhase::BEGIN);
}

::testing::AssertionResult LogContainsEndEvent(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType expected_event) {
  return LogContainsEvent(entries, offset, expected_event,
                          NetLogEventPhase::END);
}

::testing::AssertionResult LogContainsEntryWithType(
    const std::vector<NetLogEntry>& entries,
    int offset,
    NetLogEventType type) {
  size_t index = GetIndex(entries, offset);
  if (index >= entries.size())
    return ::testing::AssertionFailure() << index << " is out of bounds.";
  const NetLogEntry& entry = entries[index];
  if (entry.type != type)
    return ::testing::AssertionFailure() << "Type does not match.";
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult LogContainsEntryWithTypeAfter(
    const std::vector<NetLogEntry>& entries,
    int start_offset,
    NetLogEventType type) {
  for (size_t i = GetIndex(entries, start_offset); i < entries.size(); ++i) {
    const NetLogEntry& entry = entries[i];
    if (entry.type == type)
      return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure();
}

size_t ExpectLogContainsSomewhere(const std::vector<NetLogEntry>& entries,
                                  size_t min_offset,
                                  NetLogEventType expected_event,
                                  NetLogEventPhase expected_phase) {
  size_t min_index = GetIndex(entries, min_offset);
  size_t i = 0;
  for (; i < entries.size(); ++i) {
    const NetLogEntry& entry = entries[i];
    if (entry.type == expected_event && entry.phase == expected_phase)
      break;
  }
  EXPECT_LT(i, entries.size());
  EXPECT_GE(i, min_index);
  return i;
}

size_t ExpectLogContainsSomewhereAfter(const std::vector<NetLogEntry>& entries,
                                       size_t start_offset,
                                       NetLogEventType expected_event,
                                       NetLogEventPhase expected_phase) {
  size_t i = GetIndex(entries, start_offset);
  for (; i < entries.size(); ++i) {
    const NetLogEntry& entry = entries[i];
    if (entry.type == expected_event && entry.phase == expected_phase)
      break;
  }
  EXPECT_LT(i, entries.size());
  return i;
}

base::Optional<std::string> GetOptionalStringValueFromParams(
    const NetLogEntry& entry,
    base::StringPiece name) {
  if (!entry.params.is_dict())
    return base::nullopt;

  const std::string* result = entry.params.FindStringKey(name);
  if (!result)
    return base::nullopt;

  return *result;
}

base::Optional<bool> GetOptionalBooleanValueFromParams(const NetLogEntry& entry,
                                                       base::StringPiece name) {
  if (!entry.params.is_dict())
    return base::nullopt;
  return entry.params.FindBoolKey(name);
}

base::Optional<int> GetOptionalIntegerValueFromParams(const NetLogEntry& entry,
                                                      base::StringPiece name) {
  if (!entry.params.is_dict())
    return base::nullopt;
  return entry.params.FindIntKey(name);
}

base::Optional<int> GetOptionalNetErrorCodeFromParams(
    const NetLogEntry& entry) {
  return GetOptionalIntegerValueFromParams(entry, "net_error");
}

std::string GetStringValueFromParams(const NetLogEntry& entry,
                                     base::StringPiece name) {
  auto result = GetOptionalStringValueFromParams(entry, name);
  if (!result) {
    ADD_FAILURE() << "No string parameter " << name;
    return "";
  }
  return *result;
}

int GetIntegerValueFromParams(const NetLogEntry& entry,
                              base::StringPiece name) {
  auto result = GetOptionalIntegerValueFromParams(entry, name);
  if (!result) {
    ADD_FAILURE() << "No int parameter " << name;
    return -1;
  }
  return *result;
}

bool GetBooleanValueFromParams(const NetLogEntry& entry,
                               base::StringPiece name) {
  auto result = GetOptionalBooleanValueFromParams(entry, name);
  if (!result) {
    ADD_FAILURE() << "No bool parameter " << name;
    return -1;
  }
  return *result;
}

int GetNetErrorCodeFromParams(const NetLogEntry& entry) {
  auto result = GetOptionalNetErrorCodeFromParams(entry);
  if (!result) {
    ADD_FAILURE() << "No net_error parameter";
    return -1;
  }
  return *result;
}

bool GetListValueFromParams(const NetLogEntry& entry,
                            base::StringPiece name,
                            const base::ListValue** value) {
  if (!entry.params.is_dict())
    return false;

  const base::Value* list = entry.params.FindListKey(name);
  if (!list)
    return false;

  return list->GetAsList(value);
}

}  // namespace net

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_TRACKED_DICTIONARY_H_
#define CONTENT_WEB_TEST_COMMON_TRACKED_DICTIONARY_H_

#include <string_view>

#include "base/values.h"

namespace content {

// TrackedDictionary wraps base::DictValue, but forces all mutations to go
// through TrackedDictionary's Set methods.  This allows tracking of changes
// accumulated since the last call to ResetChangeTracking.
class TrackedDictionary {
 public:
  TrackedDictionary();

  TrackedDictionary(const TrackedDictionary&) = delete;
  TrackedDictionary& operator=(const TrackedDictionary&) = delete;

  // Current value of the tracked dictionary.
  const base::DictValue& current_values() const { return current_values_; }

  // Subset of |current_values| that have been changed (i.e. via Set method)
  // since the last call to ResetChangeTracking.
  const base::DictValue& changed_values() const { return changed_values_; }

  // Clears out |changed_values|.
  void ResetChangeTracking();

  // Overwrites |current_values| with values present in |new_changes|.
  // The new values are not present in |changed_values| afterwards.
  void ApplyUntrackedChanges(const base::DictValue& new_changes);
  void ApplyUntrackedChanges(base::DictValue&& new_changes);

  // Sets a value in |current_values| and tracks the change in |changed_values|.
  void Set(std::string_view path, base::Value new_value);

  // Type-specific setter for convenience.
  void SetBoolean(std::string_view path, bool new_value);
  void SetInteger(std::string_view path, int new_value);
  void SetString(std::string_view path, std::string_view new_value);

 private:
  base::DictValue current_values_;
  base::DictValue changed_values_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_COMMON_TRACKED_DICTIONARY_H_

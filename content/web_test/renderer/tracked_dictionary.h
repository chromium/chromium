// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_TRACKED_DICTIONARY_H_
#define CONTENT_WEB_TEST_RENDERER_TRACKED_DICTIONARY_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"

namespace content {

// TrackedDictionary wraps base::DictionaryValue, but forces all mutations to go
// through TrackedDictionary's Set methods.  This allows tracking of changes
// accumulated since the last call to ResetChangeTracking.
class TrackedDictionary {
 public:
  TrackedDictionary();

  // Current value of the tracked dictionary.
  const base::DictionaryValue& current_values() const {
    return current_values_;
  }

  // Subset of |current_values| that have been changed (i.e. via Set method)
  // since the last call to ResetChangeTracking.
  const base::DictionaryValue& changed_values() const {
    return changed_values_;
  }

  // Clears out |changed_values|.
  void ResetChangeTracking();

  // Overwrites |current_values| with values present in |new_changes|.
  // The new values are not present in |changed_values| afterwards.
  void ApplyUntrackedChanges(const base::DictionaryValue& new_changes);

  // Sets a value in |current_values| and tracks the change in |changed_values|.
  void Set(const std::string& path, std::unique_ptr<base::Value> new_value);

  // Type-specific setter for convenience.
  void SetBoolean(const std::string& path, bool new_value);
  void SetString(const std::string& path, const std::string& new_value);

 private:
  base::DictionaryValue current_values_;
  base::DictionaryValue changed_values_;

  DISALLOW_COPY_AND_ASSIGN(TrackedDictionary);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_TRACKED_DICTIONARY_H_

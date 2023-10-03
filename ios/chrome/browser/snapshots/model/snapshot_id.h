// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_H_

#include <cstdint>

// Wraps a int32 that is used to uniquely identify a snapshot. This is a
// distinct type to allow the compiler to detect incorrect usage of the
// API.
//
// The identifier needs to be stable across application restart as it is
// used to generate the filename for the file used to store the snapshot.
class SnapshotID {
 public:
  // Constructors.
  constexpr SnapshotID() : identifier_(0) {}
  constexpr explicit SnapshotID(int32_t identifier) : identifier_(identifier) {}

  // Returns whether the identifier is valid.
  constexpr bool valid() const { return identifier_ != 0; }

  // Returns the wrapped value. Use for serialization only.
  constexpr int32_t identifier() const { return identifier_; }

 private:
  int32_t identifier_;
};

// Ordering function used for sorted containers.
constexpr bool operator<(SnapshotID lhs, SnapshotID rhs) {
  return lhs.identifier() < rhs.identifier();
}

// Equality comparison function.
constexpr bool operator==(SnapshotID lhs, SnapshotID rhs) {
  return lhs.identifier() == rhs.identifier();
}

// Inequality comparison function.
constexpr bool operator!=(SnapshotID lhs, SnapshotID rhs) {
  return lhs.identifier() != rhs.identifier();
}

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_ID_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/model/proto_util.h"

#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ios::proto {
namespace {

// Equality operator for `google::protobuf::RepeatedPtrField<T>` for use
// in tests.
template <typename T>
bool operator==(const google::protobuf::RepeatedPtrField<T>& lhs,
                const google::protobuf::RepeatedPtrField<T>& rhs) {
  auto lhs_iter = lhs.begin();
  auto rhs_iter = rhs.begin();
  const auto lhs_end = lhs.end();
  const auto rhs_end = rhs.end();
  for (; lhs_iter != lhs_end && rhs_iter != rhs_end; ++lhs_iter, ++rhs_iter) {
    if (*lhs_iter != *rhs_iter) {
      return false;
    }
  }

  return lhs_iter == lhs_end && rhs_iter == rhs_end;
}

// Inequality operator for `google::protobuf::RepeatedPtrField<T>` for use
// in tests.
template <typename T>
bool operator!=(const google::protobuf::RepeatedPtrField<T>& lhs,
                const google::protobuf::RepeatedPtrField<T>& rhs) {
  return !(lhs == rhs);
}

}  // namespace

bool operator==(const WebStateListStorage& lhs,
                const WebStateListStorage& rhs) {
  if (lhs.active_index() != rhs.active_index()) {
    return false;
  }

  if (lhs.pinned_item_count() != rhs.pinned_item_count()) {
    return false;
  }

  if (lhs.items() != rhs.items()) {
    return false;
  }

  return true;
}

bool operator!=(const WebStateListStorage& lhs,
                const WebStateListStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const WebStateListItemStorage& lhs,
                const WebStateListItemStorage& rhs) {
  if (lhs.identifier() != rhs.identifier()) {
    return false;
  }

  if (lhs.opener() != rhs.opener()) {
    return false;
  }

  return true;
}

bool operator!=(const WebStateListItemStorage& lhs,
                const WebStateListItemStorage& rhs) {
  return !(lhs == rhs);
}

bool operator==(const OpenerStorage& lhs, const OpenerStorage& rhs) {
  if (lhs.index() != rhs.index()) {
    return false;
  }

  if (lhs.navigation_index() != rhs.navigation_index()) {
    return false;
  }

  return true;
}

bool operator!=(const OpenerStorage& lhs, const OpenerStorage& rhs) {
  return !(lhs == rhs);
}

}  // namespace ios::proto

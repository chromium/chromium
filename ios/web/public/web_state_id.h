// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_ID_H_
#define IOS_WEB_PUBLIC_WEB_STATE_ID_H_

#include <functional>
#include <iosfwd>

#include "components/sessions/core/session_id.h"

namespace web {

// Uniquely identifies a web state on the current device.
class WebStateID {
 public:
  // Creates an invalid WebStateID instance.
  constexpr WebStateID() = default;

  // Returns a WebStateID based on the SessionID identifier.
  constexpr static WebStateID FromSessionID(SessionID session_id) {
    return WebStateID(session_id.id());
  }

  // Returns a new unique WebStateID.
  static WebStateID NewUnique();

  // Should be used rarely because it can lead to collisions.
  constexpr static WebStateID FromSerializedValue(int32_t value) {
    return WebStateID::FromSessionID(SessionID::FromSerializedValue(value));
  }

  // Returns whether a certain underlying ID value would represent a valid
  // instance. Note that zero is also considered invalid.
  static constexpr bool IsValidValue(int32_t value) { return value > 0; }

  // Returns whether the identifier is valid.
  constexpr bool valid() const { return IsValidValue(identifier_); }

  // Returns the wrapped value.
  constexpr int32_t identifier() const { return identifier_; }

  // Converts a WebStateID to a SessionID. It is an error to call this method if
  // `valid()` returns false.
  SessionID ToSessionID() const;

 private:
  template <typename T>
  friend struct std::hash;

  constexpr explicit WebStateID(int32_t identifier) : identifier_(identifier) {}

  int32_t identifier_ = 0;
};

// Equality comparison function.
constexpr bool operator==(WebStateID lhs, WebStateID rhs) {
  return lhs.identifier() == rhs.identifier();
}

// Inequality comparison function.
constexpr bool operator!=(WebStateID lhs, WebStateID rhs) {
  return lhs.identifier() != rhs.identifier();
}

// Ordering function used for sorted containers.
constexpr bool operator<(WebStateID lhs, WebStateID rhs) {
  return lhs.identifier() < rhs.identifier();
}

// To work with CHECK-s and logs.
std::ostream& operator<<(std::ostream& out, WebStateID id);

}  // namespace web

namespace std {
template <>
struct hash<web::WebStateID> {
  size_t operator()(const web::WebStateID& web_state_id) const noexcept {
    using hasher = std::hash<decltype(web_state_id.identifier_)>;
    return hasher{}(web_state_id.identifier_);
  }
};
}  // namespace std

#endif  // IOS_WEB_PUBLIC_WEB_STATE_ID_H_

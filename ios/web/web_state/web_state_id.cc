// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_state_id.h"

#include <ostream>

#include "base/check.h"

namespace web {

// static
WebStateID WebStateID::NewUnique() {
  return WebStateID::FromSessionID(SessionID::NewUnique());
}

SessionID WebStateID::ToSessionID() const {
  CHECK(valid());
  return SessionID::FromSerializedValue(identifier_);
}

std::ostream& operator<<(std::ostream& out, web::WebStateID id) {
  return out << id.identifier();
}

}  // namespace web

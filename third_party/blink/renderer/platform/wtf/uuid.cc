// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/uuid.h"

#include "base/guid.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace WTF {

String CreateCanonicalUUIDString() {
  std::string uuid = base::GenerateGUID();
  return String::FromUTF8(uuid.data(), uuid.length());
}

bool IsValidUUID(const String& uuid) {
  // In most (if not all) cases the given uuid should be utf-8, so this
  // conversion should be almost no-op.
  StringUTF8Adaptor utf8(uuid);
  return base::IsValidGUIDOutputString(utf8.AsStringPiece());
}

}  // namespace WTF

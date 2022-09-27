// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/uuid.h"

#include "base/guid.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace WTF {

String CreateCanonicalUUIDString() {
  String uuid(base::GenerateGUID());
  DCHECK(uuid.IsLowerASCII());
  return uuid;
}

bool IsValidUUID(const String& uuid) {
  // In most (if not all) cases the given uuid should be utf-8, so this
  // conversion should be almost no-op.
  StringUTF8Adaptor utf8(uuid);
  return base::IsValidGUIDOutputString(utf8.AsStringPiece());
}

}  // namespace WTF

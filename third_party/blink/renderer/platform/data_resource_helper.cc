// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/data_resource_helper.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

String UncompressResourceAsString(int resource_id) {
  Vector<char> blob = UncompressResourceAsBinary(resource_id);
  // Vector::data() may return nullptr. We'd like to return an empty string,
  // not a null string.
  if (blob.size() > 0u)
    return String::FromUTF8(blob.data(), blob.size());
  return g_empty_string;
}

String UncompressResourceAsASCIIString(int resource_id) {
  Vector<char> blob = UncompressResourceAsBinary(resource_id);
  // Vector::data() may return nullptr. We'd like to return an empty string,
  // not a null string.
  if (blob.size() > 0u)
    return String(blob.data(), blob.size());
  return g_empty_string;
}

Vector<char> UncompressResourceAsBinary(int resource_id) {
  WebData data = Platform::Current()->UncompressDataResource(resource_id);
  const SharedBuffer& shared_buffer = data;
  return shared_buffer.CopyAs<Vector<char>>();
}

}  // namespace blink

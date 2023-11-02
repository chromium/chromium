// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/data_resource_helper.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String UncompressResourceAsString(int resource_id) {
  std::string data = Platform::Current()->GetDataResourceString(resource_id);
  return String::FromUTF8(data);
}

String UncompressResourceAsASCIIString(int resource_id) {
  std::string data = Platform::Current()->GetDataResourceString(resource_id);
  String result(data.data(), data.size());
  DCHECK(result.ContainsOnlyASCIIOrEmpty());
  return result;
}

Vector<char> UncompressResourceAsBinary(int resource_id) {
  std::string data = Platform::Current()->GetDataResourceString(resource_id);
  Vector<char> result;
  result.Append(data.data(), static_cast<wtf_size_t>(data.size()));
  return result;
}

}  // namespace blink

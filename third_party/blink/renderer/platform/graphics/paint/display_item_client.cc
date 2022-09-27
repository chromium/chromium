// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#endif

namespace blink {

String DisplayItemClient::ToString() const {
#if DCHECK_IS_ON()
  return String::Format("%p:%s", this, DebugName().Utf8().c_str());
#else
  return String::Format("%p", this);
#endif
}

std::ostream& operator<<(std::ostream& out, const DisplayItemClient& client) {
  return out << client.ToString();
}

std::ostream& operator<<(std::ostream& out, const DisplayItemClient* client) {
  if (!client)
    return out << "<null>";
  return out << *client;
}

}  // namespace blink

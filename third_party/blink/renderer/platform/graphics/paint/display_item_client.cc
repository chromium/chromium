// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/display_item_client.h"

#if DCHECK_IS_ON()
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#endif

namespace blink {

#if DCHECK_IS_ON()

HashSet<const DisplayItemClient*>* g_live_display_item_clients = nullptr;

void DisplayItemClient::OnCreate() {
  if (!g_live_display_item_clients)
    g_live_display_item_clients = new HashSet<const DisplayItemClient*>();
  g_live_display_item_clients->insert(this);
}

void DisplayItemClient::OnDestroy() {
  g_live_display_item_clients->erase(this);
}

bool DisplayItemClient::IsAlive() const {
  return g_live_display_item_clients &&
         g_live_display_item_clients->Contains(this);
}

String DisplayItemClient::SafeDebugName(bool known_to_be_safe) const {
  if (known_to_be_safe) {
    DCHECK(IsAlive());
    return DebugName();
  }

  // If the caller is not sure, we must ensure the client is alive, and it's
  // not a destroyed client at the same address of a new client.
  if (IsJustCreated())
    return "Just created:" + DebugName();
  if (IsAlive())
    return DebugName();
  return "DEAD";
}

#endif  // DCHECK_IS_ON()

String DisplayItemClient::ToString() const {
#if DCHECK_IS_ON()
  return String::Format("%p:%s", this, SafeDebugName().Utf8().c_str());
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

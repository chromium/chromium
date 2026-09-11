// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_

#include <optional>

#include "content/browser/indexed_db/indexed_db_client_state_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

// Factory for providing a callback that checks whether an IndexedDB client
// document is currently in an inactive state (e.g. in BFCache or frozen) and
// keeps it active while necessary.
class CONTENT_EXPORT IndexedDBClientStateCheckerFactory {
 public:
  // Returns a repeating callback bound to the UI thread that performs client
  // state checks and manages keep-active scopes on the associated
  // RenderFrameHost.
  static ::content::DisallowInactiveClientCallback
  GetClientStateCheckerCallback();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_

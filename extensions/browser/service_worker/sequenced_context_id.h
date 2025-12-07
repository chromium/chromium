// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SEQUENCED_CONTEXT_ID_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SEQUENCED_CONTEXT_ID_H_

#include <string>

#include "base/unguessable_token.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// Uniquely identifies a service worker context for a specific activation of an
// extension within a browser context.
struct SequencedContextId {
  ExtensionId extension_id;
  std::string browser_context_id;
  base::UnguessableToken token;

  auto operator<=>(const SequencedContextId& rhs) const = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SEQUENCED_CONTEXT_ID_H_

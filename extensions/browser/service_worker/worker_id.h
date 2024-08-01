// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

#include <stdint.h>

#include <optional>
#include <ostream>

#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace extensions {

// Identifies a running extension Service Worker.
struct WorkerId {
  WorkerId();

  WorkerId(const ExtensionId& extension_id,
           int render_process_id,
           int64_t version_id,
           int thread_id);

  WorkerId(const ExtensionId& extension_id,
           int render_process_id,
           int64_t version_id,
           int thread_id,
           const blink::ServiceWorkerToken& start_token);

  ExtensionId extension_id;
  int render_process_id;
  int64_t version_id;
  int thread_id;

  // A token that uniquely identifies this running instance of the Service
  // Worker. It is valid from worker started until stopped.
  // TODO(crbug.com/40276609): See if we can remove `render_process_id`,
  // `version_id`, and/or `thread_id` in favor of start_token. Then make this
  // non-optional when creating an instance.
  std::optional<blink::ServiceWorkerToken> start_token;

  bool operator<(const WorkerId& other) const;
  bool operator==(const WorkerId& other) const;
  bool operator!=(const WorkerId& other) const;
};

std::ostream& operator<<(std::ostream& out, const WorkerId& id);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

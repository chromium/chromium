// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

#include <stdint.h>

#include <ostream>

#include "extensions/common/extension_id.h"

namespace extensions {

// Identifies a running extension Service Worker.
struct WorkerId {
  ExtensionId extension_id;
  int render_process_id;
  int64_t version_id;
  int thread_id;

  bool operator<(const WorkerId& other) const;
  bool operator==(const WorkerId& other) const;
  bool operator!=(const WorkerId& other) const;
};

std::ostream& operator<<(std::ostream& out, const WorkerId& id);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

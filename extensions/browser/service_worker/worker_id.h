// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

#include <stdint.h>

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

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_WORKER_ID_H_

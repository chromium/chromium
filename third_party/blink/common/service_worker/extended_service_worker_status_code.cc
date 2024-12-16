// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/extended_service_worker_status_code.h"

#include "base/notreached.h"

namespace blink {

const char* ExtendedServiceWorkerStatusToString(
    ExtendedServiceWorkerStatusCode status) {
  switch (status) {
    case ExtendedServiceWorkerStatusCode::kUnknown:
      return "Unknown status";
  }
  NOTREACHED();
}

}  // namespace blink

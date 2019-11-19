// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_

#include <string>

#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"

namespace content {
namespace content_index {

// Records the result of DB operation identified by |name|.
// |name| must be one of ContentIndexDatabaseTask.
void RecordDatabaseOperationStatus(const std::string& name,
                                   blink::ServiceWorkerStatusCode status);

// Records the status of dispatching the `contentdelete` event.
// |phase| must be one of ContentIndexDispatchPhase.
void RecordDisptachStatus(const std::string& phase,
                          blink::ServiceWorkerStatusCode status);

// Records the category of a blocked entry.
void RecordRegistrationBlocked(blink::mojom::ContentCategory category);

}  // namespace content_index
}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_METRICS_H_

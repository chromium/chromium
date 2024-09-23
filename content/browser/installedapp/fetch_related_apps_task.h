// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_APPS_TASK_H_
#define CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_APPS_TASK_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"

class GURL;

namespace content {

using FetchRelatedAppsTaskResult =
    std::vector<blink::mojom::RelatedApplicationPtr>;
using FetchRelatedAppsTaskCallback =
    base::OnceCallback<void(FetchRelatedAppsTaskResult)>;

// This task is used by InstalledAppProviderImpl to fetch related apps
// from a specific platform.
class FetchRelatedAppsTask {
 public:
  virtual ~FetchRelatedAppsTask() = default;
  virtual void Start(
      const GURL& frame_url,
      std::vector<blink::mojom::RelatedApplicationPtr> related_applications,
      FetchRelatedAppsTaskCallback done_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_APPS_TASK_H_

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WEB_APPS_TASK_H_
#define CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WEB_APPS_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/installedapp/fetch_related_apps_task.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"

namespace content {

class BrowserContext;

// It will match the given related apps list against the locally installed
// web apps.
class CONTENT_EXPORT FetchRelatedWebAppsTask : public FetchRelatedAppsTask {
 public:
  explicit FetchRelatedWebAppsTask(BrowserContext* browser_context);
  FetchRelatedWebAppsTask(const FetchRelatedWebAppsTask&) = delete;
  FetchRelatedWebAppsTask& operator=(const FetchRelatedWebAppsTask&) = delete;
  ~FetchRelatedWebAppsTask() override;

  void Start(
      const GURL& frame_url,
      std::vector<blink::mojom::RelatedApplicationPtr> related_applications,
      FetchRelatedAppsTaskCallback done_callback) override;

  raw_ptr<BrowserContext> browser_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WEB_APPS_TASK_H_

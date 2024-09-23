// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WIN_APPS_TASK_H_
#define CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WIN_APPS_TASK_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/installedapp/fetch_related_apps_task.h"
#include "content/browser/installedapp/installed_app_provider_impl.h"
#include "content/browser/installedapp/native_win_app_fetcher.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-forward.h"
#include "url/gurl.h"

namespace content {

// Windows specific implementation of InstalledNativeAppProvider.
// It will match the given related apps list against the Windows verified
// application list.
class CONTENT_EXPORT FetchRelatedWinAppsTask : public FetchRelatedAppsTask {
 public:
  explicit FetchRelatedWinAppsTask(
      std::unique_ptr<NativeWinAppFetcher> native_win_app_fetcher);
  FetchRelatedWinAppsTask(const FetchRelatedWinAppsTask&) = delete;
  FetchRelatedWinAppsTask& operator=(const FetchRelatedWinAppsTask&) = delete;
  ~FetchRelatedWinAppsTask() override;

  void Start(
      const GURL& frame_url,
      std::vector<blink::mojom::RelatedApplicationPtr> related_applications,
      FetchRelatedAppsTaskCallback done_callback) override;

 private:
  void OnGetInstalledRelatedWinApps(
      std::vector<blink::mojom::RelatedApplicationPtr>
          installed_related_app_list);

  std::vector<blink::mojom::RelatedApplicationPtr> related_applications_;
  FetchRelatedAppsTaskCallback done_callback_;
  std::unique_ptr<NativeWinAppFetcher> native_win_app_fetcher_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FetchRelatedWinAppsTask> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INSTALLEDAPP_FETCH_RELATED_WIN_APPS_TASK_H_

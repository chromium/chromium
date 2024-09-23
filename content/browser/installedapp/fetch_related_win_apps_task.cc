// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/fetch_related_win_apps_task.h"

#include <windows.foundation.h>

#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/win/async_operation.h"
#include "base/win/core_winrt_util.h"
#include "base/win/post_async_results.h"
#include "base/win/vector.h"
#include "content/browser/installedapp/native_win_app_fetcher_impl.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

namespace {
constexpr char kWindowsPlatformName[] = "windows";

std::optional<blink::mojom::RelatedApplicationPtr> MatchApplication(
    const std::vector<blink::mojom::RelatedApplicationPtr>& installed_apps_list,
    blink::mojom::RelatedApplicationPtr application) {
  if (application->platform != kWindowsPlatformName) {
    return std::nullopt;
  }

  if (!application->id.has_value()) {
    return std::nullopt;
  }

  if (installed_apps_list.empty()) {
    return std::nullopt;
  }

  for (auto& installed_app : installed_apps_list) {
    // Alphanumeric AppModelUserId.
    // https://learn.microsoft.com/en-us/windows/win32/shell/appids
    if (base::CompareCaseInsensitiveASCII(application->id.value(),
                                          installed_app->id.value()) == 0) {
      return installed_app->Clone();
    }
  }

  return std::nullopt;
}
}  // namespace

FetchRelatedWinAppsTask::FetchRelatedWinAppsTask(
    std::unique_ptr<NativeWinAppFetcher> native_win_app_fetcher)
    : native_win_app_fetcher_(std::move(native_win_app_fetcher)) {}
FetchRelatedWinAppsTask::~FetchRelatedWinAppsTask() = default;

void FetchRelatedWinAppsTask::Start(
    const GURL& frame_url,
    std::vector<blink::mojom::RelatedApplicationPtr> related_applications,
    FetchRelatedAppsTaskCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  done_callback_ = std::move(done_callback);
  related_applications_ = std::move(related_applications);
  native_win_app_fetcher_->FetchAppsForUrl(
      frame_url,
      base::BindOnce(&FetchRelatedWinAppsTask::OnGetInstalledRelatedWinApps,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FetchRelatedWinAppsTask::OnGetInstalledRelatedWinApps(
    std::vector<blink::mojom::RelatedApplicationPtr>
        installed_related_app_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<blink::mojom::RelatedApplicationPtr> matched_app_list;

  for (auto& related_app : related_applications_) {
    std::optional<blink::mojom::RelatedApplicationPtr> matched_app_result =
        MatchApplication(installed_related_app_list, std::move(related_app));
    if (matched_app_result.has_value()) {
      matched_app_list.push_back(std::move(matched_app_result.value()));
    }
  }

  std::move(done_callback_).Run(std::move(matched_app_list));
}

}  // namespace content

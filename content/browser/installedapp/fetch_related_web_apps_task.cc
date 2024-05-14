// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/fetch_related_web_apps_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/concurrent_callbacks.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

namespace {
constexpr char kWebAppPlatformName[] = "webapp";

// Filters out nullopt from |matched_app_results| and returns result.
FetchRelatedAppsTaskResult RemoveNullResults(
    std::vector<std::optional<blink::mojom::RelatedApplication>>
        matched_app_results) {
  std::vector<blink::mojom::RelatedApplicationPtr> applications;

  for (const std::optional<blink::mojom::RelatedApplication>& match :
       matched_app_results) {
    if (match.has_value()) {
      // We must clone instead of move to create a type
      // blink::mojom::RelatedApplicationPtr for the result.
      applications.push_back(match->Clone());
    }
  }

  return applications;
}

}  // namespace

FetchRelatedWebAppsTask::FetchRelatedWebAppsTask(
    BrowserContext* browser_context)
    : browser_context_(browser_context) {}
FetchRelatedWebAppsTask::~FetchRelatedWebAppsTask() = default;

void FetchRelatedWebAppsTask::Start(
    const GURL& frame_url,
    std::vector<blink::mojom::RelatedApplicationPtr> related_applications,
    FetchRelatedAppsTaskCallback done_callback) {
  base::ConcurrentCallbacks<std::optional<blink::mojom::RelatedApplication>>
      concurrent;

  for (auto& related_app : related_applications) {
    if (!related_app->id.has_value()) {
      continue;
    }

    if (related_app->platform != kWebAppPlatformName) {
      continue;
    }

    GURL manifest_id(related_app->id.value());

    if (!manifest_id.is_valid()) {
      continue;
    }

    GetContentClient()->browser()->QueryInstalledWebAppsByManifestId(
        frame_url, manifest_id, browser_context_, concurrent.CreateCallback());
  }

  std::move(concurrent)
      .Done(base::BindOnce(&RemoveNullResults).Then(std::move(done_callback)));
}

}  // namespace content

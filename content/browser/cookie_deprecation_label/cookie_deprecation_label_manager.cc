// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

const base::FeatureParam<std::string> kCookieDeprecationLabel{
    &net::features::kCookieDeprecationFacilitatedTestingLabels, "label", ""};

}  // namespace

CookieDeprecationLabelManager::CookieDeprecationLabelManager(
    BrowserContext* browser_context)
    : browser_context_(*browser_context) {}

CookieDeprecationLabelManager::~CookieDeprecationLabelManager() = default;

absl::optional<std::string> CookieDeprecationLabelManager::GetValue() {
  if (!GetContentClient()->browser()->IsCookieDeprecationLabelAllowed(
          &*browser_context_)) {
    return absl::nullopt;
  }

  return GetValueInternal();
}

absl::optional<std::string> CookieDeprecationLabelManager::GetValue(
    const url::Origin& top_frame_origin,
    const url::Origin& context_origin) {
  if (!GetContentClient()->browser()->IsCookieDeprecationLabelAllowedForContext(
          &*browser_context_, top_frame_origin, context_origin)) {
    return absl::nullopt;
  }

  return GetValueInternal();
}

std::string CookieDeprecationLabelManager::GetValueInternal() {
  if (!label_value_.has_value()) {
    label_value_ = kCookieDeprecationLabel.Get();
  }

  return *label_value_;
}

}  // namespace content

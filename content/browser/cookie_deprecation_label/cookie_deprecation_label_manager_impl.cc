// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cookie_deprecation_label/cookie_deprecation_label_manager_impl.h"

#include <optional>
#include <string>

#include "base/metrics/field_trial_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"

namespace content {

CookieDeprecationLabelManagerImpl::CookieDeprecationLabelManagerImpl(
    BrowserContext* browser_context)
    : browser_context_(*browser_context) {}

CookieDeprecationLabelManagerImpl::~CookieDeprecationLabelManagerImpl() =
    default;

std::optional<std::string> CookieDeprecationLabelManagerImpl::GetValue() {
  if (!GetContentClient()->browser()->IsCookieDeprecationLabelAllowed(
          &*browser_context_)) {
    return std::nullopt;
  }

  return GetValueInternal();
}

std::optional<std::string> CookieDeprecationLabelManagerImpl::GetValue(
    const url::Origin& top_frame_origin,
    const url::Origin& context_origin) {
  if (!GetContentClient()->browser()->IsCookieDeprecationLabelAllowedForContext(
          &*browser_context_, top_frame_origin, context_origin)) {
    return std::nullopt;
  }

  return GetValueInternal();
}

std::optional<std::string>
CookieDeprecationLabelManagerImpl::GetValueInternal() {
  if (!label_value_.has_value()) {
    label_value_ = features::kCookieDeprecationLabel.Get();
  }

  return *label_value_;
}

}  // namespace content

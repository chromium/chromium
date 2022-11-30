// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader_delegate.h"

#include "content/browser/loader/navigation_early_hints_manager.h"

namespace content {

NavigationURLLoaderDelegate::EarlyHints::EarlyHints() = default;

NavigationURLLoaderDelegate::EarlyHints::~EarlyHints() = default;

NavigationURLLoaderDelegate::EarlyHints::EarlyHints(
    NavigationURLLoaderDelegate::EarlyHints&& other) = default;

NavigationURLLoaderDelegate::EarlyHints&
NavigationURLLoaderDelegate::EarlyHints::operator=(
    NavigationURLLoaderDelegate::EarlyHints&& other) = default;

}  // namespace content

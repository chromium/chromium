// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/optimization_guide/tab_url_provider_impl.h"

#include "base/notreached.h"

TabUrlProviderImpl::TabUrlProviderImpl() = default;
TabUrlProviderImpl::~TabUrlProviderImpl() = default;

const std::vector<GURL> TabUrlProviderImpl::GetUrlsOfActiveTabs(
    const base::TimeDelta& duration_since_last_shown) {
  NOTIMPLEMENTED();
  return std::vector<GURL>();
}
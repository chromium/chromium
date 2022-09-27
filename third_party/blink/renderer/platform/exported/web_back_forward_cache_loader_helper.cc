// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_back_forward_cache_loader_helper.h"

#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"

namespace blink {

WebBackForwardCacheLoaderHelper::WebBackForwardCacheLoaderHelper(
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
    : private_(back_forward_cache_loader_helper) {}

void WebBackForwardCacheLoaderHelper::Reset() {
  private_.Reset();
}

void WebBackForwardCacheLoaderHelper::Assign(
    const WebBackForwardCacheLoaderHelper& other) {
  private_ = other.private_;
}

BackForwardCacheLoaderHelper*
WebBackForwardCacheLoaderHelper::GetBackForwardCacheLoaderHelper() const {
  if (!private_)
    return nullptr;
  return private_.Get();
}

}  // namespace blink

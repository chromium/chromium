// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_BACK_FORWARD_CACHE_LOADER_HELPER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_BACK_FORWARD_CACHE_LOADER_HELPER_H_

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/platform_export.h"  // nogncheck
#endif  // INSIDE_BLINK

namespace blink {

class BackForwardCacheLoaderHelper;

// A container for passing around a reference to BackForwardCacheLoaderHelper.
class WebBackForwardCacheLoaderHelper {
 public:
  WebBackForwardCacheLoaderHelper() = default;
  ~WebBackForwardCacheLoaderHelper() { Reset(); }

  WebBackForwardCacheLoaderHelper(const WebBackForwardCacheLoaderHelper& o) {
    Assign(o);
  }
  WebBackForwardCacheLoaderHelper& operator=(
      const WebBackForwardCacheLoaderHelper& o) {
    Assign(o);
    return *this;
  }

  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebBackForwardCacheLoaderHelper&);

#if INSIDE_BLINK
  PLATFORM_EXPORT explicit WebBackForwardCacheLoaderHelper(
      BackForwardCacheLoaderHelper* back_forward_cache_loader_helper);

  BackForwardCacheLoaderHelper* GetBackForwardCacheLoaderHelper() const;
#endif

 private:
  WebPrivatePtr<BackForwardCacheLoaderHelper> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_BACK_FORWARD_CACHE_LOADER_HELPER_H_

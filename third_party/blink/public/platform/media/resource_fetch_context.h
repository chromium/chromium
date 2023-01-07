// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_RESOURCE_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_RESOURCE_FETCH_CONTEXT_H_

#include <memory>

#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"

namespace blink {

class BLINK_PLATFORM_EXPORT ResourceFetchContext {
 public:
  virtual ~ResourceFetchContext() {}

  virtual std::unique_ptr<WebAssociatedURLLoader> CreateUrlLoader(
      const WebAssociatedURLLoaderOptions& options) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_RESOURCE_FETCH_CONTEXT_H_

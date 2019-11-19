// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/public/common/resource_usage_reporter.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/platform/web_cache.h"

namespace mojo {

template <>
struct CONTENT_EXPORT TypeConverter<content::mojom::ResourceTypeStatsPtr,
                                    blink::WebCacheResourceTypeStats> {
  static content::mojom::ResourceTypeStatsPtr Convert(
      const blink::WebCacheResourceTypeStats& obj);
};

template <>
struct CONTENT_EXPORT TypeConverter<blink::WebCacheResourceTypeStats,
                                    content::mojom::ResourceTypeStats> {
  static blink::WebCacheResourceTypeStats Convert(
      const content::mojom::ResourceTypeStats& obj);
};

}  // namespace mojo

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_USAGE_REPORTER_TYPE_CONVERTERS_H_

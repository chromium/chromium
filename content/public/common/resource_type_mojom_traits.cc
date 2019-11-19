// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/resource_type_mojom_traits.h"

namespace mojo {

// static
content::mojom::ResourceType
EnumTraits<content::mojom::ResourceType, content::ResourceType>::ToMojom(
    content::ResourceType input) {
  switch (input) {
    case content::ResourceType::kMainFrame:
      return content::mojom::ResourceType::kMainFrame;
    case content::ResourceType::kSubFrame:
      return content::mojom::ResourceType::kSubFrame;
    case content::ResourceType::kStylesheet:
      return content::mojom::ResourceType::kStylesheet;
    case content::ResourceType::kScript:
      return content::mojom::ResourceType::kScript;
    case content::ResourceType::kImage:
      return content::mojom::ResourceType::kImage;
    case content::ResourceType::kFontResource:
      return content::mojom::ResourceType::kFontResource;
    case content::ResourceType::kSubResource:
      return content::mojom::ResourceType::kSubResource;
    case content::ResourceType::kObject:
      return content::mojom::ResourceType::kObject;
    case content::ResourceType::kMedia:
      return content::mojom::ResourceType::kMedia;
    case content::ResourceType::kWorker:
      return content::mojom::ResourceType::kWorker;
    case content::ResourceType::kSharedWorker:
      return content::mojom::ResourceType::kSharedWorker;
    case content::ResourceType::kPrefetch:
      return content::mojom::ResourceType::kPrefetch;
    case content::ResourceType::kFavicon:
      return content::mojom::ResourceType::kFavicon;
    case content::ResourceType::kXhr:
      return content::mojom::ResourceType::kXhr;
    case content::ResourceType::kPing:
      return content::mojom::ResourceType::kPing;
    case content::ResourceType::kServiceWorker:
      return content::mojom::ResourceType::kServiceWorker;
    case content::ResourceType::kCspReport:
      return content::mojom::ResourceType::kCspReport;
    case content::ResourceType::kPluginResource:
      return content::mojom::ResourceType::kPluginResource;
    case content::ResourceType::kNavigationPreloadMainFrame:
      return content::mojom::ResourceType::kNavigationPreloadMainFrame;
    case content::ResourceType::kNavigationPreloadSubFrame:
      return content::mojom::ResourceType::kNavigationPreloadSubFrame;
  }

  NOTREACHED();
  return content::mojom::ResourceType::kMainFrame;
}
// static
bool EnumTraits<content::mojom::ResourceType, content::ResourceType>::FromMojom(

    content::mojom::ResourceType input,
    content::ResourceType* output) {
  switch (input) {
    case content::mojom::ResourceType::kMainFrame:
      *output = content::ResourceType::kMainFrame;
      return true;
    case content::mojom::ResourceType::kSubFrame:
      *output = content::ResourceType::kSubFrame;
      return true;
    case content::mojom::ResourceType::kStylesheet:
      *output = content::ResourceType::kStylesheet;
      return true;
    case content::mojom::ResourceType::kScript:
      *output = content::ResourceType::kScript;
      return true;
    case content::mojom::ResourceType::kImage:
      *output = content::ResourceType::kImage;
      return true;
    case content::mojom::ResourceType::kFontResource:
      *output = content::ResourceType::kFontResource;
      return true;
    case content::mojom::ResourceType::kSubResource:
      *output = content::ResourceType::kSubResource;
      return true;
    case content::mojom::ResourceType::kObject:
      *output = content::ResourceType::kObject;
      return true;
    case content::mojom::ResourceType::kMedia:
      *output = content::ResourceType::kMedia;
      return true;
    case content::mojom::ResourceType::kWorker:
      *output = content::ResourceType::kWorker;
      return true;
    case content::mojom::ResourceType::kSharedWorker:
      *output = content::ResourceType::kSharedWorker;
      return true;
    case content::mojom::ResourceType::kPrefetch:
      *output = content::ResourceType::kPrefetch;
      return true;
    case content::mojom::ResourceType::kFavicon:
      *output = content::ResourceType::kFavicon;
      return true;
    case content::mojom::ResourceType::kXhr:
      *output = content::ResourceType::kXhr;
      return true;
    case content::mojom::ResourceType::kPing:
      *output = content::ResourceType::kPing;
      return true;
    case content::mojom::ResourceType::kServiceWorker:
      *output = content::ResourceType::kServiceWorker;
      return true;
    case content::mojom::ResourceType::kCspReport:
      *output = content::ResourceType::kCspReport;
      return true;
    case content::mojom::ResourceType::kPluginResource:
      *output = content::ResourceType::kPluginResource;
      return true;
    case content::mojom::ResourceType::kNavigationPreloadMainFrame:
      *output = content::ResourceType::kNavigationPreloadMainFrame;
      return true;
    case content::mojom::ResourceType::kNavigationPreloadSubFrame:
      *output = content::ResourceType::kNavigationPreloadSubFrame;
      return true;
  }
  return false;
}

}  // namespace mojo

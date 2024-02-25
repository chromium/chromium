// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/webapps_client.h"

#include <algorithm>
#include <memory>

#include "base/auto_reset.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

namespace webapps {

namespace {
WebappsClient* g_instance = nullptr;
}

WebappsClient::WebappsClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

WebappsClient::~WebappsClient() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

// static
WebappsClient* WebappsClient::Get() {
  return g_instance;
}

WebappsClient::ScopedSegmentationServiceOverride
WebappsClient::OverrideSegmentationServiceForTesting(
    std::unique_ptr<segmentation_platform::SegmentationPlatformService>
        service) {
  return ScopedSegmentationServiceOverride(&segmentation_platform_for_testing_,
                                           std::move(service));
}

}  // namespace webapps

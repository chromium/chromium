// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_PROVIDER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_PROVIDER_H_

#include <memory>

namespace content {

class AttributionManager;
class WebContents;

// Provides access to an `AttributionManager` implementation. This layer of
// abstraction is to allow tests to mock out the `AttributionManager` without
// injecting a manager explicitly.
class AttributionManagerProvider {
 public:
  // Provides access to the manager owned by the default `StoragePartition`.
  static std::unique_ptr<AttributionManagerProvider> Default();

  virtual ~AttributionManagerProvider() = default;

  // Gets the `AttributionManager` that should be used for handling attributions
  // that occur in the given `web_contents`. Returns `nullptr` if attribution
  // reporting is not enabled in the given `web_contents`, e.g. when the
  // browser context is off the record.
  virtual AttributionManager* GetManager(WebContents* web_contents) const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_MANAGER_PROVIDER_H_

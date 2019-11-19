// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_CONTENT_BROWSER_MANIFEST_H_
#define CONTENT_PUBLIC_APP_CONTENT_BROWSER_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace content {

// Returns the service manifest for the "content_browser" service. There are
// multiple instances of this service embedded within the browser process: one
// instance per BrowserContext which can be used to connect to service instances
// isolated on a per-BrowserContext basis.
//
// In-process services whose instances should each be bound to the lifetime of
// some BrowserContext are typically packaged within this manifest. For all
// other packaged services, see content_packaged_services_manifest.cc.
const service_manager::Manifest& GetContentBrowserManifest();

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_CONTENT_BROWSER_MANIFEST_H_

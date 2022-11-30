// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
#define CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

#include "content/public/browser/web_ui_data_source.h"

namespace content {

// Creates a data source for for chrome://resources/ URLs.
WebUIDataSource* CreateSharedResourcesDataSource();

// Creates a data source for for chrome-untrusted://resources/ URLs.
WebUIDataSource* CreateUntrustedSharedResourcesDataSource();

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_
#define CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

namespace content {

class WebUIDataSource;

// Populates a data source for chrome(-untrusted)://resources/ URLs.
void PopulateSharedResourcesDataSource(WebUIDataSource* source);

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_SHARED_RESOURCES_DATA_SOURCE_H_

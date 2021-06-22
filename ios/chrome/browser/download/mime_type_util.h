// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_

#include <string>

// MIME type for iOS configuration file.
extern char kMobileConfigurationType[];

// MIME type for pass data.
extern char kPkPassMimeType[];

// MIME Type for 3D models.
extern char kUsdzMimeType[];
// Legacy USDZ content types.
extern char kLegacyUsdzMimeType[];
extern char kLegacyPixarUsdzMimeType[];

// Returns whether the content-type or the file extension match those of a USDZ
// 3D model. The file extension is checked in addition to the content-type since
// many static file hosting services do not allow setting the content-type.
bool IsUsdzFileFormat(const std::string& mime_type,
                      const std::u16string& suggested_filename);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_

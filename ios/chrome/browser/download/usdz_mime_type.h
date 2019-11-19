// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_

#include <string>

#include "base/strings/string16.h"

// Universal Scene Description file format used to represent 3D models.
// See https://www.iana.org/assignments/media-types/model/vnd.usdz+zip
extern char kUsdzMimeType[];
// Legacy USDZ content types.
extern char kLegacyUsdzMimeType[];
extern char kLegacyPixarUsdzMimeType[];

// Returns whether the content-type or the file extension match those of a USDZ
// 3D model. The file extension is checked in addition to the content-type since
// many static file hosting services do not allow setting the content-type.
bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::string16& suggested_filename);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_USDZ_MIME_TYPE_H_

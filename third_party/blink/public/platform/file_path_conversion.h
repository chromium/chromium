// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_FILE_PATH_CONVERSION_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_FILE_PATH_CONVERSION_H_

#include "third_party/blink/public/platform/web_common.h"

#if INSIDE_BLINK
#include "third_party/blink/renderer/platform/wtf/forward.h"  // nogncheck
#endif

namespace base {
class FilePath;
}

namespace blink {

class WebString;

BLINK_PLATFORM_EXPORT base::FilePath WebStringToFilePath(const WebString&);

BLINK_PLATFORM_EXPORT WebString FilePathToWebString(const base::FilePath&);

#if INSIDE_BLINK
BLINK_PLATFORM_EXPORT base::FilePath StringToFilePath(const String& str);
BLINK_PLATFORM_EXPORT String FilePathToString(const base::FilePath&);
#endif

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_FILE_PATH_CONVERSION_H_

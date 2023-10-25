// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_ERRORS_H_
#define CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_ERRORS_H_

#include "content/common/content_export.h"
#include "content/common/gin_java_bridge.mojom-shared.h"

namespace content {

CONTENT_EXPORT const char* GinJavaBridgeErrorToString(
    mojom::GinJavaBridgeError error);

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_GIN_JAVA_BRIDGE_ERRORS_H_

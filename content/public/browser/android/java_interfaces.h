// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_JAVA_INTERFACES_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_JAVA_INTERFACES_H_

#include "content/common/content_export.h"

namespace service_manager {
class InterfaceProvider;
}

namespace content {

// Returns an InterfaceProvider for global Java-implemented interfaces.
// This provides access to interfaces implemented in Java in the browser process
// to C++ code in the browser process. This and the returned InterfaceProvider
// may only be used on the UI thread.
CONTENT_EXPORT service_manager::InterfaceProvider* GetGlobalJavaInterfaces();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_JAVA_INTERFACES_H_

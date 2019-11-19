// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_CHILD_DWRITE_FONT_PROXY_INIT_WIN_H_
#define CONTENT_PUBLIC_CHILD_DWRITE_FONT_PROXY_INIT_WIN_H_

#include "content/common/content_export.h"

namespace content {

// Initializes the dwrite font proxy.
CONTENT_EXPORT void InitializeDWriteFontProxy();

// Uninitialize the dwrite font proxy. This is safe to call even if the proxy
// has not been initialized. After this, calls to load fonts may fail.
CONTENT_EXPORT void UninitializeDWriteFontProxy();

}  // namespace content

#endif  // CONTENT_PUBLIC_CHILD_DWRITE_FONT_PROXY_INIT_WIN_H_

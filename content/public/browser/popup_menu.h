// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_POPUP_MENU_H_
#define CONTENT_PUBLIC_BROWSER_POPUP_MENU_H_

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content {

#if BUILDFLAG(IS_APPLE)

// On the Mac/Blink iOS, the menus shown by <select> HTML elements are native
// menus. This call allows the embedder to turn off those menus entirely, so
// that attempting to invoke them will return immediately. This is a one-way
// switch and is irrevocable.
CONTENT_EXPORT void DontShowPopupMenus();

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_POPUP_MENU_H_

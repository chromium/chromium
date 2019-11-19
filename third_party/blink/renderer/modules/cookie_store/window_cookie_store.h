// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_WINDOW_COOKIE_STORE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_WINDOW_COOKIE_STORE_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CookieStore;
class LocalDOMWindow;

// Exposes a CookieStore as the "cookieStore" attribute on the Window global.
class WindowCookieStore {
  STATIC_ONLY(WindowCookieStore);

 public:
  static CookieStore* cookieStore(LocalDOMWindow&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_STORE_WINDOW_COOKIE_STORE_H_

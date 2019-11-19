// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_

#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Document;

class CookieJar {
 public:
  explicit CookieJar(blink::Document* document);
  ~CookieJar();

  void SetCookie(const String& value);
  String Cookies();
  bool CookiesEnabled();

 private:
  void RequestRestrictedCookieManagerIfNeeded();

  mojo::Remote<network::mojom::blink::RestrictedCookieManager> backend_;
  WeakPersistent<blink::Document> document_;  // Document owns |this|.
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_

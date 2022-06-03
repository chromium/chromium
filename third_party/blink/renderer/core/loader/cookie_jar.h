// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_

#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Document;

class CookieJar : public GarbageCollected<CookieJar> {
 public:
  explicit CookieJar(blink::Document* document);
  virtual ~CookieJar();
  void Trace(Visitor* visitor) const;

  void SetCookie(const String& value);
  String Cookies();
  bool CookiesEnabled();
  void SetCookieManager(
      mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
          cookie_manager);

 private:
  bool RequestRestrictedCookieManagerIfNeeded();

  HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend_;
  Member<blink::Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_

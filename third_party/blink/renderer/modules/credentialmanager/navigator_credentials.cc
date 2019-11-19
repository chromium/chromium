// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanager/navigator_credentials.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/credentialmanager/credentials_container.h"

namespace blink {

NavigatorCredentials::NavigatorCredentials(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

NavigatorCredentials& NavigatorCredentials::From(Navigator& navigator) {
  NavigatorCredentials* supplement =
      Supplement<Navigator>::From<NavigatorCredentials>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorCredentials>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

const char NavigatorCredentials::kSupplementName[] = "NavigatorCredentials";

CredentialsContainer* NavigatorCredentials::credentials(Navigator& navigator) {
  return NavigatorCredentials::From(navigator).credentials();
}

CredentialsContainer* NavigatorCredentials::credentials() {
  if (!credentials_container_)
    credentials_container_ = MakeGarbageCollected<CredentialsContainer>();
  return credentials_container_.Get();
}

void NavigatorCredentials::Trace(blink::Visitor* visitor) {
  visitor->Trace(credentials_container_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/netinfo/navigator_network_information.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/netinfo/network_information.h"

namespace blink {

NavigatorNetworkInformation::NavigatorNetworkInformation(Navigator& navigator)
    : ContextClient(navigator.GetFrame()) {}

NavigatorNetworkInformation& NavigatorNetworkInformation::From(
    Navigator& navigator) {
  NavigatorNetworkInformation* supplement =
      ToNavigatorNetworkInformation(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorNetworkInformation>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

NavigatorNetworkInformation*
NavigatorNetworkInformation::ToNavigatorNetworkInformation(
    Navigator& navigator) {
  return Supplement<Navigator>::From<NavigatorNetworkInformation>(navigator);
}

const char NavigatorNetworkInformation::kSupplementName[] =
    "NavigatorNetworkInformation";

NetworkInformation* NavigatorNetworkInformation::connection(
    Navigator& navigator) {
  return NavigatorNetworkInformation::From(navigator).connection();
}

NetworkInformation* NavigatorNetworkInformation::connection() {
  if (!connection_ && GetFrame()) {
    DCHECK(GetFrame()->DomWindow());
    connection_ = MakeGarbageCollected<NetworkInformation>(
        GetFrame()->DomWindow()->GetExecutionContext());
  }
  return connection_.Get();
}

void NavigatorNetworkInformation::Trace(blink::Visitor* visitor) {
  visitor->Trace(connection_);
  Supplement<Navigator>::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink

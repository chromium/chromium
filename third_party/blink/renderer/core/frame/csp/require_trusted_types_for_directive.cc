// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/require_trusted_types_for_directive.h"

namespace blink {

network::mojom::blink::CSPRequireTrustedTypesFor CSPRequireTrustedTypesForParse(
    const String& value,
    ContentSecurityPolicy* policy) {
  Vector<String> list;
  value.SimplifyWhiteSpace().Split(' ', false, list);

  network::mojom::blink::CSPRequireTrustedTypesFor result =
      network::mojom::blink::CSPRequireTrustedTypesFor::None;

  for (const String& v : list) {
    // The only value in the sink group is 'script'.
    // https://w3c.github.io/webappsec-trusted-types/dist/spec/#trusted-types-sink-group
    if (v == "'script'") {
      result = network::mojom::blink::CSPRequireTrustedTypesFor::Script;
    } else {
      policy->ReportInvalidRequireTrustedTypesFor(v);
    }
  }
  if (result == network::mojom::blink::CSPRequireTrustedTypesFor::None) {
    policy->ReportInvalidRequireTrustedTypesFor(String());
  }

  return result;
}

}  // namespace blink

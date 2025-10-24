// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/connection_allowlist.h"

#include "base/containers/contains.h"
#include "services/network/public/cpp/connection_allowlist.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/policy_container.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

bool ShouldBlockRequestViaConnectionAllowlist(ExecutionContext* context,
                                              const KURL& url) {
  network::ConnectionAllowlists allowlists =
      context->GetPolicyContainer()->GetPolicies().connection_allowlists;

  // TODO(447954811): Sending reports for violations of both enforced and
  // report-only allowlists will force us to revisit this.
  if (!allowlists.enforced.has_value()) {
    return false;
  }

  // Since we're concerned about network connections, punt on local schemes:
  scoped_refptr<SecurityOrigin> origin = SecurityOrigin::Create(url);
  if (origin->IsLocal()) {
    return false;
  }

  // These should be treated as URLPatterns. For the moment, we're just treating
  // them as serialized origins and doing a direct match rather than anything
  // complicated.
  //
  // TODO(447954811): Come back to this once we have more agreement on the
  // mechanism. Ideally, we'll treat these strings as URLPatterns, but also
  // compile them _once_ (even more ideally in the network stack, though that
  // will be somewhat difficult to do while also supporting regex).
  String serialized_origin = origin->ToString();
  if (base::Contains(allowlists.enforced->allowlist,
                     origin->ToString().Ascii())) {
    return false;
  }

  return true;
}

}  // namespace blink

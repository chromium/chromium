// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_SOURCE_H_

#include "services/network/public/mojom/content_security_policy.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class KURL;

CORE_EXPORT
bool CSPSourceIsSchemeOnly(const network::mojom::blink::CSPSource& source);

CORE_EXPORT
bool CSPSourceMatches(const network::mojom::blink::CSPSource& source,
                      const String& self_protocol,
                      const KURL& url,
                      ResourceRequest::RedirectStatus =
                          ResourceRequest::RedirectStatus::kNoRedirect);

CORE_EXPORT
bool CSPSourceMatchesAsSelf(const network::mojom::blink::CSPSource& source,
                            const KURL& url);

}  // namespace blink

#endif

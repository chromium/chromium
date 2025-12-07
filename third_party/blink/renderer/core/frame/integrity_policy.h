// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_POLICY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_POLICY_H_

#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace network {
struct IntegrityPolicy;
}

namespace blink {

class KURL;
class ExecutionContext;
class DOMWrapperWorld;
struct IntegrityMetadataSet;

class IntegrityPolicy {
 public:
  CORE_EXPORT
  static bool AllowRequest(
      ExecutionContext* context,
      const DOMWrapperWorld* world,
      network::mojom::RequestDestination request_destination,
      network::mojom::RequestMode request_mode,
      const IntegrityMetadataSet& integrity_metadata,
      const KURL& url);
  static void LogParsingErrorsIfAny(ExecutionContext* context,
                                    const network::IntegrityPolicy& policy);
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_INTEGRITY_POLICY_H_

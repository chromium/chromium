// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/null_resource_fetcher_properties.h"

#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/security_context/insecure_request_policy.mojom-blink.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/https_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

NullResourceFetcherProperties::NullResourceFetcherProperties()
    : fetch_client_settings_object_(
          *MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
              KURL(),
              KURL(),
              nullptr /* security_origin */,
              network::mojom::ReferrerPolicy::kDefault,
              String(),
              HttpsState::kNone,
              AllowedByNosniff::MimeTypeCheck::kStrict,
              mojom::blink::InsecureRequestPolicy::kLeaveInsecureRequestsAlone,
              FetchClientSettingsObject::InsecureNavigationsSet())) {}

void NullResourceFetcherProperties::Trace(Visitor* visitor) const {
  visitor->Trace(fetch_client_settings_object_);
  ResourceFetcherProperties::Trace(visitor);
}

}  // namespace blink

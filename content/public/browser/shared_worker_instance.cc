// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/shared_worker_instance.h"

#include "base/check.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

SharedWorkerInstance::SharedWorkerInstance(
    const GURL& url,
    blink::mojom::ScriptType script_type,
    network::mojom::CredentialsMode credentials_mode,
    const std::string& name,
    const blink::StorageKey& creator_storage_key,
    const blink::StorageKey& worker_storage_key,
    const url::Origin& renderer_origin,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    blink::mojom::SharedWorkerSameSiteCookies same_site_cookies,
    bool extended_lifetime)
    : url_(url),
      script_type_(script_type),
      credentials_mode_(credentials_mode),
      name_(name),
      creator_storage_key_(creator_storage_key),
      worker_storage_key_(worker_storage_key),
      renderer_origin_(renderer_origin),
      creation_context_type_(creation_context_type),
      same_site_cookies_(same_site_cookies),
      extended_lifetime_(extended_lifetime) {
  // Ensure the same-origin policy is enforced correctly.
  DCHECK(url.SchemeIs(url::kDataScheme) ||
         GetContentClient()->browser()->DoesSchemeAllowCrossOriginSharedWorker(
             creator_storage_key.origin().scheme()) ||
         creator_storage_key.origin().IsSameOriginWith(url));
  // Ensure only first-party contexts can ask for SameSite Lax/Strict cookies.
  DCHECK(creator_storage_key.IsFirstPartyContext() ||
         same_site_cookies == blink::mojom::SharedWorkerSameSiteCookies::kNone);
}

SharedWorkerInstance::SharedWorkerInstance(const SharedWorkerInstance& other) =
    default;

SharedWorkerInstance::SharedWorkerInstance(SharedWorkerInstance&& other) =
    default;

SharedWorkerInstance::~SharedWorkerInstance() = default;

bool SharedWorkerInstance::Matches(
    const GURL& url,
    const std::string& name,
    const blink::StorageKey& creator_storage_key,
    const blink::mojom::SharedWorkerSameSiteCookies same_site_cookies) const {
  // Step 11.2: "If there exists a SharedWorkerGlobalScope object whose closing
  // flag is false, constructor origin is same origin with outside settings's
  // origin, constructor url equals urlRecord, and name equals the value of
  // options's name member, then set worker global scope to that
  // SharedWorkerGlobalScope object."
  if (creator_storage_key_ != creator_storage_key || url_ != url ||
      name_ != name || same_site_cookies_ != same_site_cookies) {
    return false;
  }

  // TODO(crbug.com/40554285): file:// URLs should be treated as opaque
  // origins, but not in url::Origin. Therefore, we manually check it here.
  if (url.SchemeIsFile() ||
      creator_storage_key.origin().scheme() == url::kFileScheme) {
    return false;
  }

  return true;
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
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
    const url::Origin& constructor_origin,
    network::mojom::IPAddressSpace creation_address_space,
    blink::mojom::SharedWorkerCreationContextType creation_context_type)
    : url_(url),
      script_type_(script_type),
      credentials_mode_(credentials_mode),
      name_(name),
      constructor_origin_(constructor_origin),
      creation_address_space_(creation_address_space),
      creation_context_type_(creation_context_type) {
  // Ensure the same-origin policy is enforced correctly.
  DCHECK(url.SchemeIs(url::kDataScheme) ||
         GetContentClient()->browser()->DoesSchemeAllowCrossOriginSharedWorker(
             constructor_origin.scheme()) ||
         url::Origin::Create(url).IsSameOriginWith(constructor_origin));
}

SharedWorkerInstance::SharedWorkerInstance(const SharedWorkerInstance& other) =
    default;

SharedWorkerInstance::SharedWorkerInstance(SharedWorkerInstance&& other) =
    default;

SharedWorkerInstance::~SharedWorkerInstance() = default;

bool SharedWorkerInstance::Matches(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) const {
  // Step 11.2: "If there exists a SharedWorkerGlobalScope object whose closing
  // flag is false, constructor origin is same origin with outside settings's
  // origin, constructor url equals urlRecord, and name equals the value of
  // options's name member, then set worker global scope to that
  // SharedWorkerGlobalScope object."
  if (!constructor_origin_.IsSameOriginWith(constructor_origin) ||
      url_ != url || name_ != name) {
    return false;
  }

  // TODO(https://crbug.com/794098): file:// URLs should be treated as opaque
  // origins, but not in url::Origin. Therefore, we manually check it here.
  if (url.SchemeIsFile() || constructor_origin.scheme() == url::kFileScheme)
    return false;

  return true;
}

}  // namespace content

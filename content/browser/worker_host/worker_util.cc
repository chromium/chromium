// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_util.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

blink::StorageKey CalculateWorkerStorageKey(
    const GURL& script_url,
    const blink::StorageKey& creator_storage_key,
    bool is_opaque_origin_enabled) {
  if (script_url.SchemeIs(url::kDataScheme) && is_opaque_origin_enabled) {
    url::Origin opaque_origin =
        creator_storage_key.origin().DeriveNewOpaqueOrigin();
    if (creator_storage_key.nonce()) {
      return blink::StorageKey::CreateWithNonce(
          opaque_origin, creator_storage_key.nonce().value());
    }
    return blink::StorageKey::Create(
        opaque_origin, creator_storage_key.top_level_site(),
        blink::mojom::AncestorChainBit::kCrossSite);
  }
  return creator_storage_key;
}

url::Origin CalculateWorkerRendererOrigin(
    const GURL& script_url,
    const blink::StorageKey& worker_storage_key,
    bool is_opaque_origin_enabled) {
  if (script_url.SchemeIs(url::kDataScheme) && !is_opaque_origin_enabled) {
    return url::Origin();
  }
  return worker_storage_key.origin();
}

bool DoesCreatorAllowFileUrlSupport(
    const url::Origin& creator_origin,
    const blink::web_pref::WebPreferences* web_preferences,
    bool creator_worker_has_file_url_support) {
  if (creator_origin.scheme() != url::kFileScheme) {
    return false;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles)) {
    return true;
  }
  if (web_preferences) {
    return web_preferences->allow_file_access_from_file_urls ||
           web_preferences->allow_universal_access_from_file_urls;
  }
  return creator_worker_has_file_url_support;
}

}  // namespace content

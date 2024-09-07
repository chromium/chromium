// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"

#include <algorithm>
#include <string>

#include "net/base/schemeful_site.h"
#include "url/url_canon.h"

namespace blink {

BlinkSchemefulSite::BlinkSchemefulSite() {
  site_as_origin_ = SecurityOrigin::CreateUniqueOpaque();
}

BlinkSchemefulSite::BlinkSchemefulSite(
    scoped_refptr<const SecurityOrigin> origin)
    : BlinkSchemefulSite(net::SchemefulSite(origin->ToUrlOrigin())) {}

BlinkSchemefulSite::BlinkSchemefulSite(const url::Origin& origin)
    : BlinkSchemefulSite(net::SchemefulSite(origin)) {}

BlinkSchemefulSite::BlinkSchemefulSite(const net::SchemefulSite& site) {
  site_as_origin_ = SecurityOrigin::CreateFromUrlOrigin(site.site_as_origin_);

  // While net::SchemefulSite should correctly normalize the port value, adding
  // this DCHECK makes it easier for readers of this class to trust the
  // invariant.
  //
  // We clamp up to 0 because DefaultPortForScheme() can return -1 for
  // non-standard schemes which net::SchemefulSite stores as 0. So we need to
  // make sure our check matches.
  DCHECK(
      site_as_origin_->Port() ==
      std::max(url::DefaultPortForScheme(site_as_origin_->Protocol().Ascii()),
               0));
}

BlinkSchemefulSite::operator net::SchemefulSite() const {
  return net::SchemefulSite(site_as_origin_->ToUrlOrigin());
}

String BlinkSchemefulSite::Serialize() const {
  return site_as_origin_->ToString();
}

String BlinkSchemefulSite::GetDebugString() const {
  DCHECK(site_as_origin_);
  return "{ origin_as_site: " + Serialize() + " }";
}

// static
bool BlinkSchemefulSite::FromWire(const url::Origin& site_as_origin,
                                  BlinkSchemefulSite* out) {
  // The origin passed into this constructor may not match the
  // `site_as_origin_` used as the internal representation of the schemeful
  // site. However, a valid SchemefulSite's internal origin should result in a
  // match if used to construct another SchemefulSite. Thus, if there is a
  // mismatch here, we must indicate a failure.
  BlinkSchemefulSite candidate(site_as_origin);
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromUrlOrigin(site_as_origin);

  if (!candidate.site_as_origin_->IsSameOriginWith(security_origin.get()))
    return false;

  *out = std::move(candidate);
  return true;
}

}  // namespace blink

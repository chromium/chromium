// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/url_constants.h"

namespace content {

FencedFrameURLMapping::FencedFrameURLMapping() = default;
FencedFrameURLMapping::~FencedFrameURLMapping() = default;

absl::optional<GURL> FencedFrameURLMapping::ConvertFencedFrameURNToURL(
    GURL& urn_uuid) {
  CHECK(urn_uuid.is_valid());
  CHECK_EQ(url::kUrnScheme, urn_uuid.scheme());
  absl::optional<GURL> result = absl::nullopt;
  if (IsPresent(urn_uuid))
    result = urn_uuid_to_url_map_[urn_uuid];
  return result;
}

GURL FencedFrameURLMapping::AddFencedFrameURL(GURL& url) {
  DCHECK(url.is_valid());
  DCHECK(network::IsUrlPotentiallyTrustworthy(url));

  // Create a urn::uuid.
  GURL urn_uuid =
      GURL("urn:uuid:" + base::UnguessableToken::Create().ToString());
  CHECK(!IsPresent(urn_uuid));
  urn_uuid_to_url_map_.insert(std::pair<GURL, GURL>(urn_uuid, url));
  return urn_uuid;
}

bool FencedFrameURLMapping::IsPresent(GURL& urn_uuid) {
  return urn_uuid_to_url_map_.find(urn_uuid) != urn_uuid_to_url_map_.end();
}

}  // namespace content

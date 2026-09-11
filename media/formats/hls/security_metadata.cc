// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/security_metadata.h"

#include "url/gurl.h"

namespace media::hls {

// static
SecurityMetadata SecurityMetadata::CreateForTesting(std::string url,
                                                    bool would_taint_origin,
                                                    bool did_redirect) {
  return SecurityMetadata{
      .would_taint_origin = would_taint_origin,
      .did_redirect = did_redirect,
      .has_range_request = false,
      .response_origins = {url::Origin::Create(GURL(url))},
  };
}

SecurityMetadata SecurityMetadata::CreateForTesting(const GURL& url,
                                                    bool would_taint_origin,
                                                    bool did_redirect) {
  return SecurityMetadata{
      .would_taint_origin = would_taint_origin,
      .did_redirect = did_redirect,
      .has_range_request = false,
      .response_origins = {url::Origin::Create(url)},
  };
}

bool SecurityMetadata::IsSafeLoadFromManifestOrigin(
    const url::Origin& origin) const {
  if (response_origins.size() != 1) {
    return false;
  }
  return *response_origins.begin() == origin;
}

void SecurityMetadata::MergeFrom(const SecurityMetadata& other) {
  would_taint_origin |= other.would_taint_origin;
  did_redirect |= other.did_redirect;
  has_range_request |= other.has_range_request;
  response_origins.insert(std::begin(other.response_origins),
                          std::end(other.response_origins));
}

}  // namespace media::hls

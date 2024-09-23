// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_POLICY_H_
#define CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_POLICY_H_

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// This represents the parsed result of the Popin-Policy HTTP request header.
// The top-frame of the popin can only navigate to pages which permit the
// popin's opener origin via this policy.
// See https://explainers-by-googlers.github.io/partitioned-popins/
struct CONTENT_EXPORT PartitionedPopinsPolicy {
  explicit PartitionedPopinsPolicy(std::string untrusted_input);
  ~PartitionedPopinsPolicy();

  // Whether this policy should match any opener origin. This must be false if
  // any `origins` are set below.
  bool wildcard = false;

  // The explicit opener origins this policy permits. Every origin must be
  // https.
  std::vector<url::Origin> origins;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PARTITIONED_POPINS_PARTITIONED_POPINS_POLICY_H_

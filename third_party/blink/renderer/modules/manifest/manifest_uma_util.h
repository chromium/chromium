// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_UMA_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_UMA_UTIL_H_

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

namespace blink {

class ManifestUmaUtil {
 public:
  enum FetchFailureReason {
    FETCH_EMPTY_URL = 0,
    FETCH_FROM_OPAQUE_ORIGIN,
    FETCH_UNSPECIFIED_REASON
  };

  // Record that the Manifest was successfully parsed. If it is an empty
  // Manifest, it will recorded as so and nothing will happen. Otherwise, the
  // presence of each properties will be recorded.
  static void ParseSucceeded(const mojom::blink::ManifestPtr& manifest);

  // Record that the Manifest parsing failed.
  static void ParseFailed();

  // Record that the Manifest fetching succeeded.
  static void FetchSucceeded();

  // Record that the Manifest fetching failed and takes the |reason| why it
  // failed.
  static void FetchFailed(FetchFailureReason reason);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MANIFEST_MANIFEST_UMA_UTIL_H_

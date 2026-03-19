// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_

#include <memory>
#include <optional>

#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/platform/loader/fetch/ad_tagging_utils.h"

namespace blink {

CORE_EXPORT std::unique_ptr<protocol::Network::AdAncestry>
CreateAdAncestryProtocolObject(const AdTracker::AdScriptAncestry& ad_ancestry);

CORE_EXPORT std::unique_ptr<protocol::Network::AdProvenance>
CreateAdProvenanceProtocolObject(const Node& node,
                                 const AdProvenance& ad_provenance);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_

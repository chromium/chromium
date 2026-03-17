// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_

#include <memory>

#include "third_party/blink/renderer/core/ad_tracker/ad_tracker.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"

namespace blink {

CORE_EXPORT std::unique_ptr<protocol::Network::AdAncestry>
CreateAdAncestryProtocolObject(const AdTracker::AdScriptAncestry& ad_ancestry);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_AD_TAGGING_UTILS_H_

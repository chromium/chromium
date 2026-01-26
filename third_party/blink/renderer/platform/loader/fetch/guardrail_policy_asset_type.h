// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_GUARDRAIL_POLICY_ASSET_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_GUARDRAIL_POLICY_ASSET_TYPE_H_

namespace blink {

// Asset types under network-efficiency-guardrails criteria.
enum class GuardrailPolicyAssetType { kData, kImage };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_GUARDRAIL_POLICY_ASSET_TYPE_H_

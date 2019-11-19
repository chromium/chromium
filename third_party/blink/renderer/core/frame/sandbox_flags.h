/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SANDBOX_FLAGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SANDBOX_FLAGS_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {
using SandboxFlagFeaturePolicyPairs =
    Vector<std::pair<WebSandboxFlags, mojom::FeaturePolicyFeature>>;

// Returns a vector of pairs of sandbox flags and the corresponding feature
// policies. This includes most but not all sandbox flags as some flags have not
// yet migrated to using feature policies.
const SandboxFlagFeaturePolicyPairs& SandboxFlagsWithFeaturePolicies();

WebSandboxFlags ParseSandboxPolicy(const SpaceSplitString& policy,
                                   String& invalid_tokens_error_message);

// With FeaturePolicyForSandbox most sandbox flags will be represented with
// features. This method returns the part of sandbox flags which were not mapped
// to corresponding features.
WebSandboxFlags GetSandboxFlagsNotImplementedAsFeaturePolicy(WebSandboxFlags);

// Applies the sandbox flags as parsed feature policies; If a flag is present
// both in the provided flags and in the parsed feature as a feature policy,
// the parsed policy takes precedence.
void ApplySandboxFlagsToParsedFeaturePolicy(WebSandboxFlags,
                                            ParsedFeaturePolicy&);

}  // namespace blink

#endif

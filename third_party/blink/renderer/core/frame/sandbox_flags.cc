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

#include "third_party/blink/renderer/core/frame/sandbox_flags.h"

#include "third_party/blink/public/common/frame/sandbox_flags.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

const SandboxFlagFeaturePolicyPairs& SandboxFlagsWithFeaturePolicies() {
  DEFINE_STATIC_LOCAL(
      SandboxFlagFeaturePolicyPairs, array,
      ({{WebSandboxFlags::kTopNavigation,
         mojom::FeaturePolicyFeature::kTopNavigation},
        {WebSandboxFlags::kForms, mojom::FeaturePolicyFeature::kFormSubmission},
        {WebSandboxFlags::kScripts, mojom::FeaturePolicyFeature::kScript},
        {WebSandboxFlags::kPopups, mojom::FeaturePolicyFeature::kPopups},
        {WebSandboxFlags::kPointerLock,
         mojom::FeaturePolicyFeature::kPointerLock},
        {WebSandboxFlags::kModals, mojom::FeaturePolicyFeature::kModals},
        {WebSandboxFlags::kOrientationLock,
         mojom::FeaturePolicyFeature::kOrientationLock},
        {WebSandboxFlags::kPresentationController,
         mojom::FeaturePolicyFeature::kPresentation},
        {WebSandboxFlags::kDownloads,
         mojom::FeaturePolicyFeature::kDownloadsWithoutUserActivation}}));
  return array;
}

// This returns a super mask which indicates the set of all flags that have
// corresponding feature policies. With FeaturePolicyForSandbox, these flags
// are always removed from the set of sandbox flags set for a sandboxed
// <iframe> (those sandbox flags are now contained in the |ContainerPolicy|).
WebSandboxFlags SandboxFlagsImplementedByFeaturePolicy() {
  DEFINE_STATIC_LOCAL(WebSandboxFlags, mask, (WebSandboxFlags::kNone));
  if (mask == WebSandboxFlags::kNone) {
    for (const auto& pair : SandboxFlagsWithFeaturePolicies())
      mask |= pair.first;
  }
  return mask;
}

WebSandboxFlags ParseSandboxPolicy(const SpaceSplitString& policy,
                                   String& invalid_tokens_error_message) {
  // http://www.w3.org/TR/html5/the-iframe-element.html#attr-iframe-sandbox
  // Parse the unordered set of unique space-separated tokens.
  WebSandboxFlags flags = WebSandboxFlags::kAll;
  unsigned length = policy.size();
  unsigned number_of_token_errors = 0;
  StringBuilder token_errors;

  for (unsigned index = 0; index < length; index++) {
    // Turn off the corresponding sandbox flag if it's set as "allowed".
    String sandbox_token(policy[index]);
    if (EqualIgnoringASCIICase(sandbox_token, "allow-same-origin")) {
      flags = flags & ~WebSandboxFlags::kOrigin;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-forms")) {
      flags = flags & ~WebSandboxFlags::kForms;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-scripts")) {
      flags = flags & ~WebSandboxFlags::kScripts;
      flags = flags & ~WebSandboxFlags::kAutomaticFeatures;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-top-navigation")) {
      flags = flags & ~WebSandboxFlags::kTopNavigation;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-popups")) {
      flags = flags & ~WebSandboxFlags::kPopups;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-pointer-lock")) {
      flags = flags & ~WebSandboxFlags::kPointerLock;
    } else if (EqualIgnoringASCIICase(sandbox_token,
                                      "allow-orientation-lock")) {
      flags = flags & ~WebSandboxFlags::kOrientationLock;
    } else if (EqualIgnoringASCIICase(sandbox_token,
                                      "allow-popups-to-escape-sandbox")) {
      flags = flags & ~WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-modals")) {
      flags = flags & ~WebSandboxFlags::kModals;
    } else if (EqualIgnoringASCIICase(sandbox_token, "allow-presentation")) {
      flags = flags & ~WebSandboxFlags::kPresentationController;
    } else if (EqualIgnoringASCIICase(
                   sandbox_token, "allow-top-navigation-by-user-activation")) {
      flags = flags & ~WebSandboxFlags::kTopNavigationByUserActivation;
    } else if (EqualIgnoringASCIICase(
                   sandbox_token, "allow-downloads-without-user-activation")) {
      flags = flags & ~WebSandboxFlags::kDownloads;
    } else if (RuntimeEnabledFeatures::StorageAccessAPIEnabled() &&
               EqualIgnoringASCIICase(
                   sandbox_token, "allow-storage-access-by-user-activation")) {
      flags = flags & ~WebSandboxFlags::kStorageAccessByUserActivation;
    } else {
      token_errors.Append(token_errors.IsEmpty() ? "'" : ", '");
      token_errors.Append(sandbox_token);
      token_errors.Append("'");
      number_of_token_errors++;
    }
  }

  if (number_of_token_errors) {
    token_errors.Append(number_of_token_errors > 1
                            ? " are invalid sandbox flags."
                            : " is an invalid sandbox flag.");
    invalid_tokens_error_message = token_errors.ToString();
  }

  return flags;
}

// Removes a certain set of flags from |sandbox_flags| for which we have feature
// policies implemented.
WebSandboxFlags GetSandboxFlagsNotImplementedAsFeaturePolicy(
    WebSandboxFlags sandbox_flags) {
  // Punch all the sandbox flags which are converted to feature policy.
  return sandbox_flags & ~SandboxFlagsImplementedByFeaturePolicy();
}

void ApplySandboxFlagsToParsedFeaturePolicy(
    WebSandboxFlags sandbox_flags,
    ParsedFeaturePolicy& parsed_feature_policy) {
  for (const auto& pair : SandboxFlagsWithFeaturePolicies()) {
    if ((sandbox_flags & pair.first) != WebSandboxFlags::kNone)
      DisallowFeatureIfNotPresent(pair.second, parsed_feature_policy);
  }
}

}  // namespace blink

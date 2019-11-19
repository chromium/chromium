/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SCHEME_REGISTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SCHEME_REGISTRY_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using URLSchemesSet = HashSet<String>;

template <typename Mapped, typename MappedTraits>
using URLSchemesMap = HashMap<String,
                              Mapped,
                              DefaultHash<String>::Hash,
                              HashTraits<String>,
                              MappedTraits>;

class PLATFORM_EXPORT SchemeRegistry {
  STATIC_ONLY(SchemeRegistry);

 public:
  static void Initialize();

  static void RegisterURLSchemeAsLocal(const String&);
  static bool ShouldTreatURLSchemeAsLocal(const String&);

  static bool ShouldTreatURLSchemeAsRestrictingMixedContent(const String&);

  // Subresources transported by secure schemes do not trigger mixed content
  // warnings. For example, https and data are secure schemes because they
  // cannot be corrupted by active network attackers.
  static void RegisterURLSchemeAsSecure(const String&);
  static bool ShouldTreatURLSchemeAsSecure(const String&);

  static bool ShouldTreatURLSchemeAsNoAccess(const String&);

  // Display-isolated schemes can only be displayed (in the sense of
  // SecurityOrigin::canDisplay) by documents from the same scheme.
  static void RegisterURLSchemeAsDisplayIsolated(const String&);
  static bool ShouldTreatURLSchemeAsDisplayIsolated(const String&);

  static bool ShouldLoadURLSchemeAsEmptyDocument(const String&);

  static void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                       const String&);
  static bool IsDomainRelaxationForbiddenForURLScheme(const String&);

  // Such schemes should delegate to SecurityOrigin::canRequest for any URL
  // passed to SecurityOrigin::canDisplay.
  static bool CanDisplayOnlyIfCanRequest(const String& scheme);

  // Schemes against which javascript: URLs should not be allowed to run (stop
  // bookmarklets from running on sensitive pages).
  static void RegisterURLSchemeAsNotAllowingJavascriptURLs(
      const String& scheme);
  static void RemoveURLSchemeAsNotAllowingJavascriptURLs(const String& scheme);
  static bool ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(
      const String& scheme);

  static bool ShouldTreatURLSchemeAsCorsEnabled(const String& scheme);

  // Serialize the registered schemes in a comma-separated list.
  static String ListOfCorsEnabledURLSchemes();

  // "Legacy" schemes (e.g. 'ftp:') which we might want to treat differently
  // from "webby" schemes.
  static bool ShouldTreatURLSchemeAsLegacy(const String& scheme);

  // Does the scheme represent a location relevant to web compatibility metrics?
  static bool ShouldTrackUsageMetricsForScheme(const String& scheme);

  // Schemes that can register a service worker.
  static void RegisterURLSchemeAsAllowingServiceWorkers(const String& scheme);
  static bool ShouldTreatURLSchemeAsAllowingServiceWorkers(
      const String& scheme);

  // HTTP-like schemes that are treated as supporting the Fetch API.
  static void RegisterURLSchemeAsSupportingFetchAPI(const String& scheme);
  static bool ShouldTreatURLSchemeAsSupportingFetchAPI(const String& scheme);

  // https://fetch.spec.whatwg.org/#fetch-scheme
  static bool IsFetchScheme(const String& scheme);

  // Schemes which override the first-/third-party checks on a Document.
  // TODO(chlily): This should also reflect the fact that chrome:// scheme
  // should be considered first-party if the embedded origin is secure, to be
  // consistent with other places that check this.
  static void RegisterURLSchemeAsFirstPartyWhenTopLevel(const String& scheme);
  static void RemoveURLSchemeAsFirstPartyWhenTopLevel(const String& scheme);
  static bool ShouldTreatURLSchemeAsFirstPartyWhenTopLevel(
      const String& scheme);

  // Schemes that can be used in a referrer.
  static void RegisterURLSchemeAsAllowedForReferrer(const String& scheme);
  static void RemoveURLSchemeAsAllowedForReferrer(const String& scheme);
  static bool ShouldTreatURLSchemeAsAllowedForReferrer(const String& scheme);

  // Allow resources from some schemes to load on a page, regardless of its
  // Content Security Policy.
  enum PolicyAreas : uint32_t {
    kPolicyAreaNone = 0,
    kPolicyAreaImage = 1 << 0,
    kPolicyAreaStyle = 1 << 1,
    // Add more policy areas as needed by clients.
    kPolicyAreaAll = ~static_cast<uint32_t>(0),
  };
  static void RegisterURLSchemeAsBypassingContentSecurityPolicy(
      const String& scheme,
      PolicyAreas = kPolicyAreaAll);
  static void RemoveURLSchemeRegisteredAsBypassingContentSecurityPolicy(
      const String& scheme);
  static bool SchemeShouldBypassContentSecurityPolicy(
      const String& scheme,
      PolicyAreas = kPolicyAreaAll);

  // Schemes which bypass Secure Context checks defined in
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy
  static void RegisterURLSchemeBypassingSecureContextCheck(
      const String& scheme);
  static bool SchemeShouldBypassSecureContextCheck(const String& scheme);

  // Schemes that can use 'wasm-eval'.
  static void RegisterURLSchemeAsAllowingWasmEvalCSP(const String& scheme);
  static bool SchemeSupportsWasmEvalCSP(const String& scheme);

 private:
  static const URLSchemesSet& LocalSchemes();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SCHEME_REGISTRY_H_

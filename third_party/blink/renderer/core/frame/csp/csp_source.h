// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CSP_CSP_SOURCE_H_

#include "third_party/blink/public/platform/web_content_security_policy_struct.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContentSecurityPolicy;
class KURL;

class CORE_EXPORT CSPSource final : public GarbageCollected<CSPSource> {
 public:
  enum WildcardDisposition { kNoWildcard, kHasWildcard };

  // NotMatching is the only negative member, the rest are different types of
  // matches. NotMatching should always be 0 to let if statements work nicely
  enum class PortMatchingResult {
    kNotMatching,
    kMatchingWildcard,
    kMatchingUpgrade,
    kMatchingExact
  };

  enum class SchemeMatchingResult {
    kNotMatching,
    kMatchingUpgrade,
    kMatchingExact
  };

  CSPSource(ContentSecurityPolicy*,
            const String& scheme,
            const String& host,
            int port,
            const String& path,
            WildcardDisposition host_wildcard,
            WildcardDisposition port_wildcard);
  CSPSource(ContentSecurityPolicy* policy, const CSPSource& other);
  bool IsSchemeOnly() const;
  const String& GetScheme() { return scheme_; }
  bool Matches(const KURL&,
               ResourceRequest::RedirectStatus =
                   ResourceRequest::RedirectStatus::kNoRedirect) const;

  bool MatchesAsSelf(const KURL&);

  // Returns true if this CSPSource subsumes the other, as defined by the
  // algorithm at https://w3c.github.io/webappsec-csp/embedded/#subsume-policy
  bool Subsumes(CSPSource*) const;
  // Retrieve the most restrictive information from the two CSPSources if
  // isSimilar is true for the two. Otherwise, return nullptr.
  CSPSource* Intersect(CSPSource*) const;
  // Returns true if the first list subsumes the second, as defined by the
  // algorithm at
  // https://w3c.github.io/webappsec-csp/embedded/#subsume-source-list
  static bool FirstSubsumesSecond(const HeapVector<Member<CSPSource>>&,
                                  const HeapVector<Member<CSPSource>>&);

  WebContentSecurityPolicySourceExpression ExposeForNavigationalChecks() const;

  void Trace(blink::Visitor*);

 private:
  FRIEND_TEST_ALL_PREFIXES(CSPSourceTest, IsSimilar);
  FRIEND_TEST_ALL_PREFIXES(CSPSourceTest, Intersect);
  FRIEND_TEST_ALL_PREFIXES(CSPSourceTest, IntersectSchemesOnly);
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest, GetIntersectCSPSources);
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest,
                           GetIntersectCSPSourcesSchemes);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, GetSourceVector);
  FRIEND_TEST_ALL_PREFIXES(CSPDirectiveListTest, OperativeDirectiveGivenType);
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest, SubsumesWithSelf);
  FRIEND_TEST_ALL_PREFIXES(SourceListDirectiveTest, GetSources);

  SchemeMatchingResult SchemeMatches(const String&) const;
  bool HostMatches(const String&) const;
  bool PathMatches(const String&) const;
  // Protocol is necessary to determine default port if it is zero.
  PortMatchingResult PortMatches(int port, const String& protocol) const;
  bool IsSimilar(CSPSource* other) const;

  // Helper inline functions for Port and Scheme MatchingResult enums
  bool inline RequiresUpgrade(const PortMatchingResult result) const {
    return result == PortMatchingResult::kMatchingUpgrade;
  }
  bool inline RequiresUpgrade(const SchemeMatchingResult result) const {
    return result == SchemeMatchingResult::kMatchingUpgrade;
  }

  bool inline CanUpgrade(const PortMatchingResult result) const {
    return result == PortMatchingResult::kMatchingUpgrade ||
           result == PortMatchingResult::kMatchingWildcard;
  }

  bool inline CanUpgrade(const SchemeMatchingResult result) const {
    return result == SchemeMatchingResult::kMatchingUpgrade;
  }

  Member<ContentSecurityPolicy> policy_;
  String scheme_;
  String host_;
  int port_;
  String path_;

  WildcardDisposition host_wildcard_;
  WildcardDisposition port_wildcard_;
};

}  // namespace blink

#endif

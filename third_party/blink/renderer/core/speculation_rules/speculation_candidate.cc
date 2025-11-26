// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_candidate.h"

#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

SpeculationCandidate::SpeculationCandidate(
    const KURL& url,
    mojom::blink::SpeculationAction action,
    const Referrer& referrer,
    bool requires_anonymous_client_ip_when_cross_origin,
    mojom::blink::SpeculationTargetHint target_hint,
    mojom::blink::SpeculationEagerness eagerness,
    network::mojom::blink::NoVarySearchPtr no_vary_search,
    mojom::blink::SpeculationInjectionType injection_type,
    Vector<String> tags,
    SpeculationRuleSet* rule_set,
    HTMLAnchorElementBase* anchor,
    SpeculationRule::FormSubmission form_submission)
    : url_(url),
      action_(action),
      referrer_(std::move(referrer)),
      requires_anonymous_client_ip_when_cross_origin_(
          requires_anonymous_client_ip_when_cross_origin),
      target_hint_(target_hint),
      eagerness_(eagerness),
      no_vary_search_(std::move(no_vary_search)),
      injection_type_(injection_type),
      tags_(std::move(tags)),
      rule_set_(rule_set),
      anchor_(anchor),
      form_submission_(form_submission) {
  DCHECK(rule_set);
  DCHECK(url.ProtocolIsInHTTPFamily());
}

void SpeculationCandidate::Trace(Visitor* visitor) const {
  visitor->Trace(rule_set_);
  visitor->Trace(anchor_);
}

mojom::blink::SpeculationCandidatePtr SpeculationCandidate::ToMojom() const {
  return mojom::blink::SpeculationCandidate::New(
      url_, action_,
      mojom::blink::Referrer::New(KURL(referrer_.referrer),
                                  referrer_.referrer_policy),
      requires_anonymous_client_ip_when_cross_origin_, target_hint_, eagerness_,
      no_vary_search_.Clone(), injection_type_, tags_, form_submission_);
}

bool SpeculationCandidate::IsSimilarFromAuthorPerspectiveExceptForTags(
    const SpeculationCandidate& other) const {
  auto as_tie = [](const SpeculationCandidate& candidate) {
    return std::tie(candidate.url_, candidate.action_, candidate.referrer_,
                    candidate.requires_anonymous_client_ip_when_cross_origin_,
                    candidate.target_hint_, candidate.eagerness_,
                    candidate.no_vary_search_, candidate.injection_type_);
  };
  return as_tie(*this) == as_tie(other);
}

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_

#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// A single speculation rule which permits some set of URLs to be speculated,
// subject to some conditions.
//
// https://jeremyroman.github.io/alternate-loading-modes/#speculation-rule
class CORE_EXPORT SpeculationRule final
    : public GarbageCollected<SpeculationRule> {
 public:
  using RequiresAnonymousClientIPWhenCrossOrigin =
      base::StrongAlias<class RequiresAnonymousClientIPWhenCrossOriginTag,
                        bool>;

  SpeculationRule(Vector<KURL>, RequiresAnonymousClientIPWhenCrossOrigin);
  ~SpeculationRule();

  const Vector<KURL>& urls() const { return urls_; }
  bool requires_anonymous_client_ip_when_cross_origin() const {
    return requires_anonymous_client_ip_.value();
  }

  void Trace(Visitor*) const;

 private:
  Vector<KURL> urls_;
  RequiresAnonymousClientIPWhenCrossOrigin requires_anonymous_client_ip_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_H_

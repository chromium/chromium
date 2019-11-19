// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GLOBAL_RULE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GLOBAL_RULE_SET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/css/rule_feature_set.h"

namespace blink {

class Document;
class RuleSet;

// A per Document collection of CSS metadata used for style matching and
// invalidation. The data is aggregated from author rulesets from all TreeScopes
// in the whole Document as well as UA stylesheets and watched selectors which
// apply to elements in all TreeScopes.
//
// TODO(futhark@chromium.org): We would like to move as much of this data as
// possible to the ScopedStyleResolver as possible to avoid full reconstruction
// of these rulesets on shadow tree changes. See https://crbug.com/401359

class CSSGlobalRuleSet final : public GarbageCollected<CSSGlobalRuleSet> {
 public:
  CSSGlobalRuleSet() = default;

  void Dispose();
  void InitWatchedSelectorsRuleSet(Document&);
  void MarkDirty() { is_dirty_ = true; }
  bool IsDirty() const { return is_dirty_; }
  void Update(Document&);

  const RuleFeatureSet& GetRuleFeatureSet() const {
    CHECK(features_.IsAlive());
    return features_;
  }
  RuleSet* WatchedSelectorsRuleSet() const {
    return watched_selectors_rule_set_;
  }
  bool HasFullscreenUAStyle() const { return has_fullscreen_ua_style_; }

  void Trace(blink::Visitor*);

 private:
  // Constructed from rules in all TreeScopes including UA style and style
  // injected from extensions.
  RuleFeatureSet features_;

  // Rules injected from extensions.
  Member<RuleSet> watched_selectors_rule_set_;

  bool has_fullscreen_ua_style_ = false;
  bool is_dirty_ = true;
  DISALLOW_COPY_AND_ASSIGN(CSSGlobalRuleSet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GLOBAL_RULE_SET_H_

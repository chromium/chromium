// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_FEATURES_H_

namespace blink {

class FeatureContext;

namespace speculation_rules {

// You might be asking, why not just check RuntimeEnabledFeatures?
//
// The answer is that the REF is turned on by an origin trial (namely,
// SpeculationRulesPrefetchFuture), and that works even if the associated
// base::Feature is disabled. The simplest solution seems to be this little
// hack -- just check that the base::Feature is actually on before actually
// using the feature. This suffices because these are not used to control WebIDL
// exposure.
bool EagernessEnabled(const FeatureContext*);
bool SelectorMatchesEnabled(const FeatureContext*);

}  // namespace speculation_rules
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULES_FEATURES_H_

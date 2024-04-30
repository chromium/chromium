# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl

_CODE_GEN_EXPR_PASS_KEY = object()


class CodeGenExpr(object):
    """
    Represents an expression which is composable to produce another expression.

    This is designed primarily to represent conditional expressions and basic
    logical operators (expr_not, expr_and, expr_or) come along with.
    """

    def __init__(self, expr, is_compound=False, pass_key=None):
        assert isinstance(expr, (bool, str))
        assert isinstance(is_compound, bool)
        assert pass_key is _CODE_GEN_EXPR_PASS_KEY

        if isinstance(expr, bool):
            self._text = "true" if expr else "false"
        else:
            self._text = expr
        self._is_compound = is_compound
        self._is_always_false = expr is False
        self._is_always_true = expr is True

    def __eq__(self, other):
        if not isinstance(self, other.__class__):
            return NotImplemented
        # Assume that, as long as the two texts are the same, the two
        # expressions must be the same, i.e. |_is_compound|, etc. must be the
        # same or do not matter.
        return self.to_text() == other.to_text()

    def __ne__(self, other):
        return not (self == other)

    def __hash__(self):
        return hash(self.to_text())

    def __str__(self):
        """
        __str__ is designed to be used when composing another expression.  If
        you'd only like to have a string representation, |to_text| works better.
        """
        if self._is_compound:
            return "({})".format(self.to_text())
        return self.to_text()

    def to_text(self):
        return self._text

    @property
    def is_always_false(self):
        """
        The expression is always False, and code generators have chances of
        optimizations.
        """
        return self._is_always_false

    @property
    def is_always_true(self):
        """
        The expression is always True, and code generators have chances of
        optimizations.
        """
        return self._is_always_true


def _Expr(*args, **kwargs):
    return CodeGenExpr(*args, pass_key=_CODE_GEN_EXPR_PASS_KEY, **kwargs)


def _unary_op(op, term):
    assert isinstance(op, str)
    assert isinstance(term, CodeGenExpr)

    return _Expr("{}{}".format(op, term), is_compound=True)


def _binary_op(op, terms):
    assert isinstance(op, str)
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert all(
        not (term.is_always_false or term.is_always_true) for term in terms)

    return _Expr(op.join(map(str, terms)), is_compound=True)


def expr_not(term):
    assert isinstance(term, CodeGenExpr)

    if term.is_always_false:
        return _Expr(True)
    if term.is_always_true:
        return _Expr(False)
    return _unary_op("!", term)


def expr_and(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert terms

    if any(term.is_always_false for term in terms):
        return _Expr(False)
    terms = list(filter(lambda x: not x.is_always_true, terms))
    if not terms:
        return _Expr(True)
    terms = expr_uniq(terms)
    if len(terms) == 1:
        return terms[0]
    return _binary_op(" && ", terms)


def expr_or(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert terms

    if any(term.is_always_true for term in terms):
        return _Expr(True)
    terms = list(filter(lambda x: not x.is_always_false, terms))
    if not terms:
        return _Expr(False)
    terms = expr_uniq(terms)
    if len(terms) == 1:
        return terms[0]
    return _binary_op(" || ", terms)


def expr_uniq(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)

    uniq_terms = []
    for term in terms:
        if term not in uniq_terms:
            uniq_terms.append(term)
    return uniq_terms


def expr_from_exposure(exposure,
                       global_names=None,
                       may_use_feature_selector=False):
    """
    Returns an expression to determine whether this property should be exposed
    or not.

    Args:
        exposure: web_idl.Exposure of the target construct.
        global_names: When specified, it's taken into account that the global
            object implements |global_names|.
        may_use_feature_selector: True enables use of ${feature_selector} iff
            the exposure is context dependent.
    """
    assert isinstance(exposure, web_idl.Exposure)
    assert (global_names is None
            or (isinstance(global_names, (list, tuple))
                and all(isinstance(name, str) for name in global_names)))
    assert isinstance(may_use_feature_selector, bool)

    # The property exposures are categorized into three.
    # - Unconditional: Always exposed.
    # - Context-independent: Enabled per v8::Isolate.
    # - Context-dependent: Enabled per v8::Context, e.g. origin trials, browser
    #   controlled features.
    #
    # Context-dependent properties can be installed in two phases.
    # - The first phase installs all the properties that are associated with the
    #   features enabled at the moment.  This phase is represented by
    #   FeatureSelector as FeatureSelector.IsAll().
    # - The second phase installs the properties associated with the specified
    #   feature.  This phase is represented as FeatureSelector.IsAny(feature).
    #
    # The exposure condition is represented as;
    #   (and feature_selector-independent-term
    #        (or
    #         feature_selector-1st-phase-term
    #         feature_selector-2nd-phase-term))
    # which can be represented in more details as:
    #   (and cross_origin_isolated_term
    #        injection_mitigated_term
    #        isolated_context_term
    #        secure_context_term
    #        uncond_exposed_term
    #        (or
    #         (and feature_selector.IsAll()  # 1st phase; all enabled
    #              cond_exposed_term
    #              (or feature_enabled_term
    #                  context_enabled_term))
    #         (or exposed_selector_term      # 2nd phase; any selected
    #             feature_selector_term)))
    # where
    #   cross_origin_isolated_term represents [CrossOriginIsolated]
    #   injection_mitigated_term represents [InjectionMitigated]
    #   isolated_context_term represents [IsolatedContext]
    #   secure_context_term represents [SecureContext=F1]
    #   uncond_exposed_term represents [Exposed=(G1, G2)]
    #   cond_exposed_term represents [Exposed(G1 F1, G2 F2)]
    #   feature_enabled_term represents [RuntimeEnabled=(F1, F2)]
    #   context_enabled_term represents [ContextEnabled=F1]
    #   exposed_selector_term represents [Exposed(G1 F1, G2 F2)]
    #   feature_selector_term represents [RuntimeEnabled=(F1, F2)]
    uncond_exposed_terms = []
    cond_exposed_terms = []
    feature_enabled_terms = []
    context_enabled_terms = []
    exposed_selector_terms = []
    feature_selector_names = []  # Will turn into feature_selector.IsAnyOf(...)

    def ref_enabled(feature):
        arg = "${execution_context}" if feature.is_context_dependent else ""
        return _Expr("RuntimeEnabledFeatures::{}Enabled({})".format(
            feature, arg))

    def ref_selected(features):
        feature_tokens = map(
            lambda feature: "mojom::blink::OriginTrialFeature::k{}".format(
                feature), features)
        return _Expr("${{feature_selector}}.IsAnyOf({})".format(
            ", ".join(feature_tokens)))

    # [CrossOriginIsolated], [CrossOriginIsolatedOrRuntimeEnabled]
    if exposure.only_in_coi_contexts:
        cross_origin_isolated_term = _Expr("${is_cross_origin_isolated}")
    elif exposure.only_in_coi_contexts_or_runtime_enabled_features:
        cross_origin_isolated_term = expr_or([
            _Expr("${is_cross_origin_isolated}"),
            expr_or(
                list(
                    map(
                        ref_enabled, exposure.
                        only_in_coi_contexts_or_runtime_enabled_features)))
        ])
    else:
        cross_origin_isolated_term = _Expr(True)

    # [InjectionMitigated]
    if exposure.only_in_injection_mitigated_contexts:
        injection_mitigated_context_term = _Expr(
            "${is_in_injection_mitigated_context}")
    else:
        injection_mitigated_context_term = _Expr(True)

    # [IsolatedContext]
    if exposure.only_in_isolated_contexts:
        isolated_context_term = _Expr("${is_in_isolated_context}")
    else:
        isolated_context_term = _Expr(True)

    # [SecureContext]
    if exposure.only_in_secure_contexts is True:
        secure_context_term = _Expr("${is_in_secure_context}")
    elif exposure.only_in_secure_contexts is False:
        secure_context_term = _Expr(True)
    else:
        terms = list(map(ref_enabled, exposure.only_in_secure_contexts))
        secure_context_term = expr_or(
            [_Expr("${is_in_secure_context}"),
             expr_not(expr_and(terms))])

    # [Exposed]
    GLOBAL_NAME_TO_EXECUTION_CONTEXT_TEST = {
        "AnimationWorklet": "IsAnimationWorkletGlobalScope",
        "AudioWorklet": "IsAudioWorkletGlobalScope",
        "DedicatedWorker": "IsDedicatedWorkerGlobalScope",
        "LayoutWorklet": "IsLayoutWorkletGlobalScope",
        "PaintWorklet": "IsPaintWorkletGlobalScope",
        "ServiceWorker": "IsServiceWorkerGlobalScope",
        "ShadowRealm": "IsShadowRealmGlobalScope",
        "SharedWorker": "IsSharedWorkerGlobalScope",
        "SharedStorageWorklet": "IsSharedStorageWorkletGlobalScope",
        "Window": "IsWindow",
        "Worker": "IsWorkerGlobalScope",
        "Worklet": "IsWorkletGlobalScope",
    }
    if global_names:
        matched_global_count = 0
        for entry in exposure.global_names_and_features:
            if entry.global_name == "*":
                # [Exposed(GLOBAL_NAME FEATURE_NAME)] is not supported.
                assert entry.feature is None
                # Constructs with the wildcard exposure ([Exposed=*]) are
                # unconditionally exposed.
                pass
            elif entry.global_name not in global_names:
                continue
            matched_global_count += 1
            if entry.feature:
                cond_exposed_terms.append(ref_enabled(entry.feature))
                if entry.feature.is_origin_trial:
                    feature_selector_names.append(entry.feature)
        assert (not exposure.global_names_and_features
                or matched_global_count > 0)
    else:
        for entry in exposure.global_names_and_features:
            if entry.global_name == "*":
                # [Exposed(GLOBAL_NAME FEATURE_NAME)] is not supported.
                assert entry.feature is None
                # Constructs with the wildcard exposure ([Exposed=*]) are
                # unconditionally exposed.
                continue
            try:
                execution_context_check = GLOBAL_NAME_TO_EXECUTION_CONTEXT_TEST[
                    entry.global_name]
            except KeyError:
                # We don't currently have a general way of checking the exposure
                # of [TargetOfExposed] exposure. If this is actually a global,
                # add it to GLOBAL_NAME_TO_EXECUTION_CONTEXT_CHECK.
                return _Expr(
                    "(::logging::NotReachedError::NotReached() << "
                    "\"{} exposure test is not supported at runtime\", false)".
                    format(entry.global_name))

            pred_term = _Expr(
                "${{execution_context}}->{}()".format(execution_context_check))
            if not entry.feature:
                uncond_exposed_terms.append(pred_term)
            else:
                cond_exposed_terms.append(
                    expr_and([pred_term, ref_enabled(entry.feature)]))
                if entry.feature.is_origin_trial:
                    exposed_selector_terms.append(
                        expr_and([pred_term,
                                  ref_selected([entry.feature])]))

    # [RuntimeEnabled]
    if exposure.runtime_enabled_features:
        feature_enabled_terms.extend(
            map(ref_enabled, exposure.runtime_enabled_features))
        if exposure.origin_trial_features:
            feature_selector_names.extend(exposure.origin_trial_features)

    # [ContextEnabled]
    if exposure.context_enabled_features:
        terms = list(
            map(
                lambda feature: _Expr(
                    "${{context_feature_settings}}->is{}Enabled()".format(
                        feature)), exposure.context_enabled_features))
        context_enabled_terms.append(
            expr_and([_Expr("${context_feature_settings}"),
                      expr_or(terms)]))

    # Build an expression.
    top_level_terms = []
    top_level_terms.append(cross_origin_isolated_term)
    top_level_terms.append(injection_mitigated_context_term)
    top_level_terms.append(isolated_context_term)
    top_level_terms.append(secure_context_term)
    if uncond_exposed_terms:
        top_level_terms.append(expr_or(uncond_exposed_terms))

    if not (may_use_feature_selector
            and exposure.is_context_dependent(global_names)):
        if cond_exposed_terms:
            top_level_terms.append(expr_or(cond_exposed_terms))
        if feature_enabled_terms:
            top_level_terms.append(expr_and(feature_enabled_terms))
        if context_enabled_terms:
            top_level_terms.append(expr_or(context_enabled_terms))
        return expr_and(top_level_terms)

    all_enabled_terms = [_Expr("${feature_selector}.IsAll()")]
    if cond_exposed_terms:
        all_enabled_terms.append(expr_or(cond_exposed_terms))
    if feature_enabled_terms or context_enabled_terms:
        terms = []
        if feature_enabled_terms:
            terms.append(expr_and(feature_enabled_terms))
        if context_enabled_terms:
            terms.append(expr_or(context_enabled_terms))
        all_enabled_terms.append(expr_or(terms))

    selector_terms = []
    if exposed_selector_terms:
        selector_terms.append(expr_or(exposed_selector_terms))
    if feature_selector_names:
        # Remove duplicates
        selector_terms.append(ref_selected(sorted(
            set(feature_selector_names))))

    terms = []
    terms.append(expr_and(all_enabled_terms))
    if selector_terms:
        terms.append(expr_or(selector_terms))
    top_level_terms.append(expr_or(terms))

    return expr_and(top_level_terms)

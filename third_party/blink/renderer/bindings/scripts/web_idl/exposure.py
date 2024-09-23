# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .runtime_enabled_features import RuntimeEnabledFeatures


class _Feature(str):
    """Represents a runtime-enabled feature."""

    def __new__(cls, value):
        return str.__new__(cls, value)

    def __init__(self, value):
        str.__init__(self)
        self._is_context_dependent = (
            RuntimeEnabledFeatures.is_context_dependent(self))
        self._is_origin_trial = RuntimeEnabledFeatures.is_origin_trial(self)

    @property
    def is_context_dependent(self):
        return self._is_context_dependent

    @property
    def is_origin_trial(self):
        return self._is_origin_trial


class _GlobalNameAndFeature(object):
    def __init__(self, global_name, feature=None):
        assert isinstance(global_name, str)
        assert feature is None or isinstance(feature, str)

        self._global_name = global_name
        if feature is None:
            self._feature = None
        else:
            self._feature = _Feature(feature)

    @property
    def global_name(self):
        return self._global_name

    @property
    def feature(self):
        return self._feature


class Exposure(object):
    """Represents a set of conditions under which the construct is exposed."""

    def __init__(self, other=None):
        assert other is None or isinstance(other, Exposure)

        if other:
            self._global_names_and_features = tuple(
                other.global_names_and_features)
            self._runtime_enabled_features = tuple(
                other.runtime_enabled_features)
            self._context_independent_runtime_enabled_features = tuple(
                other.context_independent_runtime_enabled_features)
            self._context_dependent_runtime_enabled_features = tuple(
                other.context_dependent_runtime_enabled_features)
            self._origin_trial_features = tuple(other._origin_trial_features)
            self._context_enabled_features = tuple(
                other.context_enabled_features)
            self._only_in_coi_contexts = other.only_in_coi_contexts
            self._only_in_coi_contexts_or_runtime_enabled_features = tuple(
                other.only_in_coi_contexts_or_runtime_enabled_features)
            self._only_in_injection_mitigated_contexts = other.only_in_injection_mitigated_contexts
            self._only_in_isolated_contexts = other.only_in_isolated_contexts
            self._only_in_secure_contexts = other.only_in_secure_contexts
        else:
            self._global_names_and_features = tuple()
            self._runtime_enabled_features = tuple()
            self._context_independent_runtime_enabled_features = tuple()
            self._context_dependent_runtime_enabled_features = tuple()
            self._origin_trial_features = tuple()
            self._context_enabled_features = tuple()
            self._only_in_coi_contexts = False
            self._only_in_coi_contexts_or_runtime_enabled_features = tuple()
            self._only_in_injection_mitigated_contexts = False
            self._only_in_isolated_contexts = False
            self._only_in_secure_contexts = None

    @property
    def global_names_and_features(self):
        """
        Returns a list of pairs of global name and runtime enabled feature,
        which is None if not applicable.
        """
        return self._global_names_and_features

    @property
    def runtime_enabled_features(self):
        """
        Returns a list of runtime enabled features.  This construct is exposed
        only when all these features are enabled.
        """
        return self._runtime_enabled_features

    @property
    def context_independent_runtime_enabled_features(self):
        """Returns runtime enabled features that are context-independent."""
        return self._context_independent_runtime_enabled_features

    @property
    def context_dependent_runtime_enabled_features(self):
        """Returns runtime enabled features that are context-dependent."""
        return self._context_dependent_runtime_enabled_features

    @property
    def origin_trial_features(self):
        """
        Returns a list of origin trial features.
        """
        return self._origin_trial_features

    @property
    def context_enabled_features(self):
        """
        Returns a list of context enabled features.  This construct is exposed
        only when one of these features is enabled in the context.
        """
        return self._context_enabled_features

    @property
    def only_in_coi_contexts(self):
        """
        Returns whether this construct is available only in cross-origin
        isolated contexts. The returned value is a boolean: True if the
        construct is restricted to COI contexts, or False if not.

        https://webidl.spec.whatwg.org/#CrossOriginIsolated
        """
        return self._only_in_coi_contexts

    @property
    def only_in_coi_contexts_or_runtime_enabled_features(self):
        """
        Returns a list of runtime enabled features that affects cross-origin
        isolation.

        If the list is not empty, this construct is available only in
        cross-origin isolated contexts or when any of the specified runtime
        enabled features (supposed to be origin trials) gets enabled.
        """
        return self._only_in_coi_contexts_or_runtime_enabled_features

    @property
    def only_in_injection_mitigated_contexts(self):
        """
        Returns whether this construct is available only in contexts with
        sufficient injection attack mitigations. The returned value is a
        boolean: True if the construct is restricted, False otherwise.
        """
        return self._only_in_injection_mitigated_contexts

    @property
    def only_in_isolated_contexts(self):
        """
        Returns whether this construct is available only in isolated app
        contexts. The returned value is a boolean: True if the construct
        is restricted to isolated application contexts, False if not.

        TODO(crbug.com/1206150): This needs a specification (and definition).
        """
        return self._only_in_isolated_contexts

    @property
    def only_in_secure_contexts(self):
        """
        Returns whether this construct is available only in secure contexts or
        not.  The returned value is either of a boolean (True: unconditionally
        restricted in secure contexts, or False: never restricted) or a list of
        flag names (restricted only when all flags are enabled).

        https://webidl.spec.whatwg.org/#dfn-available-only-in-secure-contexts
        """
        if self._only_in_secure_contexts is None:
            return False
        return self._only_in_secure_contexts

    def is_context_dependent(self, global_names=None):
        """
        Returns True if the exposure of this construct depends on a context.

        Args:
            global_names: When specified, it's taken into account that the
                global object implements |global_names|.
        """
        assert (global_names is None
                or (isinstance(global_names, (list, tuple))
                    and all(isinstance(name, str) for name in global_names)))

        if (self.context_dependent_runtime_enabled_features
                or self.context_enabled_features or self.only_in_coi_contexts
                or self.only_in_injection_mitigated_contexts
                or self.only_in_isolated_contexts
                or self.only_in_secure_contexts):
            return True

        if not global_names:
            return bool(self.global_names_and_features)

        is_context_dependent = False
        for entry in self.global_names_and_features:
            if entry.global_name not in global_names:
                continue
            if entry.feature and entry.feature.is_context_dependent:
                is_context_dependent = True
        return is_context_dependent


class ExposureMutable(Exposure):
    def __init__(self):
        Exposure.__init__(self)

        self._global_names_and_features = []
        self._runtime_enabled_features = []
        self._context_independent_runtime_enabled_features = []
        self._context_dependent_runtime_enabled_features = []
        self._origin_trial_features = []
        self._context_enabled_features = []
        self._only_in_coi_contexts = False
        self._only_in_coi_contexts_or_runtime_enabled_features = []
        self._only_in_injection_mitigated_contexts = False
        self._only_in_isolated_contexts = False
        self._only_in_secure_contexts = None

    def __getstate__(self):
        assert False, "ExposureMutable must not be pickled."

    def __setstate__(self, state):
        assert False, "ExposureMutable must not be pickled."

    def add_global_name_and_feature(self, global_name, feature_name=None):
        self._global_names_and_features.append(
            _GlobalNameAndFeature(global_name, feature_name))

    def add_runtime_enabled_feature(self, name):
        assert isinstance(name, str)
        feature = _Feature(name)
        if feature.is_context_dependent:
            self._context_dependent_runtime_enabled_features.append(feature)
        else:
            self._context_independent_runtime_enabled_features.append(feature)
        if feature.is_origin_trial:
            self._origin_trial_features.append(feature)
        self._runtime_enabled_features.append(feature)

    def add_context_enabled_feature(self, name):
        assert isinstance(name, str)
        self._context_enabled_features.append(name)

    def set_only_in_coi_contexts(self, value):
        assert isinstance(value, bool)
        self._only_in_coi_contexts = value

    def add_only_in_coi_contexts_or_runtime_enabled_feature(self, name):
        assert isinstance(name, str)
        self._only_in_coi_contexts_or_runtime_enabled_features.append(
            _Feature(name))

    def set_only_in_injection_mitigated_contexts(self, value):
        assert isinstance(value, bool)
        self._only_in_injection_mitigated_contexts = value

    def set_only_in_isolated_contexts(self, value):
        assert isinstance(value, bool)
        self._only_in_isolated_contexts = value

    def set_only_in_secure_contexts(self, value):
        assert (isinstance(value, (bool, str))
                or (isinstance(value, (list, tuple))
                    and all(isinstance(name, str) for name in value)))
        assert self._only_in_secure_contexts is None
        if isinstance(value, bool):
            self._only_in_secure_contexts = value
        elif isinstance(value, str):
            self._only_in_secure_contexts = (_Feature(value), )
        else:
            self._only_in_secure_contexts = tuple(map(_Feature, value))

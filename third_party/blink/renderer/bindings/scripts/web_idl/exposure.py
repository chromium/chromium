# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .runtime_enabled_features import RuntimeEnabledFeatures


class _GlobalNameAndFeature(object):
    def __init__(self, global_name, feature=None):
        assert isinstance(global_name, str)
        assert feature is None or isinstance(feature, str)

        self._global_name = global_name
        self._feature = feature

    @property
    def global_name(self):
        return self._global_name

    @property
    def feature(self):
        return self._feature


class Exposure(object):
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
            self._context_enabled_features = tuple(
                other.context_enabled_features)
            self._only_in_secure_contexts = other.only_in_secure_contexts
        else:
            self._global_names_and_features = tuple()
            self._runtime_enabled_features = tuple()
            self._context_independent_runtime_enabled_features = tuple()
            self._context_dependent_runtime_enabled_features = tuple()
            self._context_enabled_features = tuple()
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
        only when one of these features is enabled.
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
    def context_enabled_features(self):
        """
        Returns a list of context enabled features.  This construct is exposed
        only when one of these features is enabled in the context.
        """
        return self._context_enabled_features

    @property
    def only_in_secure_contexts(self):
        """
        Returns whether this construct is available only in secure contexts or
        not.  The returned value is either of a boolean (True: unconditionally
        restricted in secure contexts, or False: never restricted) or a list of
        flag names (restricted only when all flags are enabled).

        https://heycam.github.io/webidl/#dfn-available-only-in-secure-contexts
        """
        if self._only_in_secure_contexts is None:
            return False
        return self._only_in_secure_contexts


class ExposureMutable(Exposure):
    def __init__(self):
        Exposure.__init__(self)

        self._global_names_and_features = []
        self._runtime_enabled_features = []
        self._context_independent_runtime_enabled_features = []
        self._context_dependent_runtime_enabled_features = []
        self._context_enabled_features = []
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
        if RuntimeEnabledFeatures.is_context_dependent(name):
            self._context_dependent_runtime_enabled_features.append(name)
        else:
            self._context_independent_runtime_enabled_features.append(name)
        self._runtime_enabled_features.append(name)

    def add_context_enabled_feature(self, name):
        assert isinstance(name, str)
        self._context_enabled_features.append(name)

    def set_only_in_secure_contexts(self, value):
        assert (isinstance(value, (bool, str))
                or (isinstance(value, (list, tuple))
                    and all(isinstance(name, str) for name in value)))
        assert self._only_in_secure_contexts is None
        if isinstance(value, bool):
            self._only_in_secure_contexts = value
        elif isinstance(value, str):
            self._only_in_secure_contexts = (value, )
        else:
            self._only_in_secure_contexts = tuple(value)

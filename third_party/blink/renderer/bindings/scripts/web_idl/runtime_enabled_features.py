# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5


class RuntimeEnabledFeatures(object):
    """Represents a set of definitions of runtime enabled features."""

    _REQUIRE_INIT_MESSAGE = (
        "RuntimeEnabledFeatures.init must be called in advance.")
    _is_initialized = False

    @classmethod
    def init(cls, filepaths):
        """
        Args:
            filepaths: Paths to the definition files of runtime-enabled features
                ("runtime_enabled_features.json5").
        """
        assert not cls._is_initialized
        assert isinstance(filepaths, list)
        assert all(isinstance(filepath, str) for filepath in filepaths)

        cls._features = {}

        for filepath in filepaths:
            with open(filepath, encoding='utf-8') as file_obj:
                datastore = json5.load(file_obj)

            for entry in datastore["data"]:
                assert entry["name"] not in cls._features
                cls._features[entry["name"]] = entry

        cls._is_initialized = True

    @classmethod
    def is_context_dependent(cls, feature_name):
        """Returns True if the feature may be enabled per-context."""
        return (cls.is_browser_controlled(feature_name)
                or cls.is_origin_trial(feature_name))

    @classmethod
    def is_browser_controlled(cls, feature_name):
        """Returns True if the feature can be controlled by the browser."""
        assert cls._is_initialized, cls._REQUIRE_INIT_MESSAGE
        assert isinstance(feature_name, str)
        assert feature_name in cls._features, (
            "Unknown runtime-enabled feature: {}".format(feature_name))
        value = cls._features[feature_name].get(
            "browser_process_read_write_access", False)
        assert isinstance(value, bool)
        return value

    @classmethod
    def is_origin_trial(cls, feature_name):
        """Returns True if the feature is an origin trial."""
        assert cls._is_initialized, cls._REQUIRE_INIT_MESSAGE
        assert isinstance(feature_name, str)
        assert feature_name in cls._features, (
            "Unknown runtime-enabled feature: {}".format(feature_name))
        return cls._features[feature_name].get(
            "origin_trial_feature_name") is not None

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .rule_base import RuleBase
from .target_type import TargetType


class RuleStore(object):
    """
    Stores rules for each target type.
    """

    def __init__(self):
        """
        `_target_rules_map` is a mapping which has a list of rules for each
        `target_type`.

        {
            target.CALLBACK_FUNCTIONS: [CallbackRule1(), CallbackRule2(), ...],
            target.INTERFACES: [InterfaceRule1(), InterfaceRule2(), ...],
            ...
        }
        """
        self._target_rules_map = {}

    def register(self, target_type, rule):
        """
        Register a rule which validates objects of the `target_type`.
        """
        assert isinstance(target_type, TargetType)
        assert isinstance(rule, RuleBase)
        self._target_rules_map.setdefault(target_type, []).append(rule)

    @property
    def all_target_types(self):
        """
        Returns all the target types registered in this class.
        """
        return self._target_rules_map.keys()

    def get_rules(self, target_type):
        """
        Returns a list of rules which validate objects of the `target_type`.
        """
        assert isinstance(target_type, TargetType)
        assert target_type in self._target_rules_map
        return self._target_rules_map[target_type]

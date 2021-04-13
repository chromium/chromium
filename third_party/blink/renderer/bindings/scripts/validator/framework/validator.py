# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl
from . import target
from .rule_base import RuleBase
from .rule_store import RuleStore
from .target_store import TargetStore
from .target_type import TargetType


class Validator(object):
    """
    Provides an API to Check if each IDL file follows rules defined in Web IDL
    by validating an instance of web_idl.Database.
    """

    def __init__(self, web_idl_database):
        """
        Instantiates with web_idl.Database.
        """
        assert isinstance(web_idl_database, web_idl.Database)
        self._web_idl_database = web_idl_database
        self._target_store = TargetStore(web_idl_database)

    def execute(self, rule_store):
        """
        Validates `_web_idl_database` follows the rules stored in `rule_store`.

        Usage:
        You can register rules to RuleStore object, and call the API below.

            validator_instance.execute(rule_store)
        """
        assert isinstance(rule_store, RuleStore)

        for target_type in rule_store.all_target_types:
            rules = rule_store.get_rules(target_type)
            target_objects = self._target_store.get(target_type)
            self._validate_each_type(target_type, rules, target_objects)

    def _validate_each_type(self, target_type, rules, target_objects):
        assert isinstance(target_type, TargetType)
        for target_object in target_objects:
            for rule in rules:
                assert isinstance(rule, RuleBase)
                if not rule.is_valid(target_object):
                    debug_info = target_type.get_debug_info(target_object)
                    self._report(debug_info, rule.error_message)

    def _report(self, debug_info, error_message):
        print("{}, line {}\n{}".format(debug_info.location.filepath,
                                       debug_info.location.line_number,
                                       error_message))

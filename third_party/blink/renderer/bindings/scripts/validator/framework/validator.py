# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl
from .rule_base import RuleBase
from .rule_store import RuleStore
from .target_store import TargetStore


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

    def execute(self, rule_store, report_error):
        """
        Validates `_web_idl_database` follows the rules stored in `rule_store`.

        Args:
          rule_store:
            A RuleStore which holds rules.
          report_error:
            A function to handle a detected error. It takes a Rule object,
            a target object, a debug_info, and an error_message.

        Returns:
          The number of validation errors.
        """
        assert isinstance(rule_store, RuleStore)

        # These local variables are captured in assert_.
        rule = None
        target_type = None
        target_object = None

        def assert_(condition, text, *args, **kwargs):
            if not condition:
                error_message = text.format(*args, **kwargs)
                report_error(rule=rule,
                             target=target_object,
                             target_type=target_type,
                             error_message=error_message)

        for target_type in rule_store.all_target_types:
            rules = rule_store.get_rules(target_type)
            target_objects = self._target_store.get(target_type)
            for target_object in target_objects:
                for rule in rules:
                    assert isinstance(rule, RuleBase)
                    rule.validate(assert_, target_object)

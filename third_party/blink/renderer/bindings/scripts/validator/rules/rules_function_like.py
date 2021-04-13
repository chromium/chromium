# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether function-like objects
(callback functions, operations in interfaces, etc) violate the rules described
in Web IDL https://heycam.github.io/webidl/.

Each rule class should inherit RuleBase.
"""

from validator.framework import target
from validator.framework import RuleBase


class NonOptionalArgumentAfterOptional(RuleBase):
    def is_valid(self, function_like):
        is_optional = False
        for argument in function_like.arguments:
            if is_optional and not argument.is_optional:
                return False
            if argument.is_optional:
                is_optional = True
        return True

    @property
    def error_message(self):
        return "Non-optional argument must not follow an optional argument"


def register_rules(rule_store):
    rule_store.register(target.FUNCTION_LIKES,
                        NonOptionalArgumentAfterOptional())

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether function-like objects
(callback functions, operations in interfaces, etc) violate the rules described
in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import target
from validator.framework import RuleBase


class NonOptionalArgumentAfterOptionalOrVariadicArgument(RuleBase):
    def validate(self, assert_, function_like):
        is_optional_seen = False
        for i, argument in enumerate(function_like.arguments):
            assert_(
                not is_optional_seen
                or (argument.is_optional or argument.is_variadic),
                ("A non-optional argument "
                 "must not follow an optional argument."))
            if argument.is_optional:
                is_optional_seen = True
            if argument.is_variadic:
                assert_(i == len(function_like.arguments) - 1,
                        ("A variadic argument must be written "
                         "at the end of arguments."))


def register_rules(rule_store):
    rule_store.register(target.FUNCTION_LIKES,
                        NonOptionalArgumentAfterOptionalOrVariadicArgument())

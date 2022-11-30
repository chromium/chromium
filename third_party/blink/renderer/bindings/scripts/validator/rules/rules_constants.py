# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether constants violate the rules
described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import target
from validator.framework import RuleBase


class IncompatibleTypeWithConstantValue(RuleBase):
    def validate(self, assert_, constant):
        assert_(constant.value.is_type_compatible_with(constant.idl_type),
                "The constant value {} is incompatible with the type `{}`.",
                constant.value.literal, constant.idl_type.type_name)


class IncompatibleTypeWithDefaultValue(RuleBase):
    def validate(self, assert_, target_object):
        if target_object.default_value:
            assert_(
                target_object.default_value.is_type_compatible_with(
                    target_object.idl_type),
                "The default value {} is incompatible with the type `{}`.",
                target_object.default_value.literal,
                target_object.idl_type.type_name)


def register_rules(rule_store):
    rule_store.register(target.CONSTANTS, IncompatibleTypeWithConstantValue())
    rule_store.register(target.ARGUMENTS, IncompatibleTypeWithDefaultValue())
    rule_store.register(target.DICTIONARY_MEMBERS,
                        IncompatibleTypeWithDefaultValue())

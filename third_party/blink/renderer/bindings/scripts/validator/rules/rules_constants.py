# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether constants violate the rules
described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import RuleBase
from validator.framework import target


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


class ForbiddenSequenceTypeForConstants(RuleBase):
    def validate(self, assert_, constant):
        assert_(not constant.idl_type.unwrap().is_sequence,
                "Sequences must not be used as the type of a constant.")


class ForbiddenRecordTypeForConstants(RuleBase):
    def validate(self, assert_, constant):
        assert_(not constant.idl_type.unwrap().is_record,
                "Records must not be used as the type of a constant.")


class ForbiddenDictionaryTypeForConstants(RuleBase):
    def validate(self, assert_, constant):
        assert_(not constant.idl_type.unwrap().is_dictionary,
                "Dictionaries must not be used as the type of a constant.")


class ForbiddenObservableArrayTypeForConstants(RuleBase):
    def validate(self, assert_, constant):
        assert_(
            not constant.idl_type.unwrap().is_observable_array,
            "Observable arrays must not be used as the type of a constant.")


def register_rules(rule_store):
    rule_store.register(target.CONSTANTS, IncompatibleTypeWithConstantValue())
    rule_store.register(target.ARGUMENTS, IncompatibleTypeWithDefaultValue())
    rule_store.register(target.DICTIONARY_MEMBERS,
                        IncompatibleTypeWithDefaultValue())
    rule_store.register(target.CONSTANTS, ForbiddenSequenceTypeForConstants())
    rule_store.register(target.CONSTANTS, ForbiddenRecordTypeForConstants())
    rule_store.register(target.CONSTANTS,
                        ForbiddenDictionaryTypeForConstants())
    rule_store.register(target.CONSTANTS,
                        ForbiddenObservableArrayTypeForConstants())

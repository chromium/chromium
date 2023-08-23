# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether dictionaries violate the rules
described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import RuleBase
from validator.framework import target


class ForbiddenNullableDictionaryTypeForArguments(RuleBase):
    def validate(self, assert_, argument):
        assert_(
            not (argument.idl_type.is_nullable
                 and argument.idl_type.unwrap().is_dictionary),
            ("Nullable dictionary type is forbidden as "
             "an argument type."))


class ForbiddenNullableDictionaryTypeForDictionaryMembers(RuleBase):
    def validate(self, assert_, dictionary_member):
        assert_(
            not (dictionary_member.idl_type.is_nullable
                 and dictionary_member.idl_type.unwrap().is_dictionary),
            ("Nullable dictionary type is forbidden as "
             "a dictionary member type."))


def register_rules(rule_store):
    rule_store.register(target.ARGUMENTS,
                        ForbiddenNullableDictionaryTypeForArguments())
    rule_store.register(target.DICTIONARY_MEMBERS,
                        ForbiddenNullableDictionaryTypeForDictionaryMembers())

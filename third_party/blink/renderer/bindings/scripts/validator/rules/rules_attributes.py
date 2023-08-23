# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether attributes violate the rules
described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import RuleBase
from validator.framework import target


class ForbiddenSequenceTypeForAttributes(RuleBase):
    def validate(self, assert_, attribute):
        assert_(not attribute.idl_type.unwrap().is_sequence,
                "Sequences must not be used as the type of an attribute.")


class ForbiddenRecordTypeForAttributes(RuleBase):
    def validate(self, assert_, attribute):
        assert_(not attribute.idl_type.unwrap().is_record,
                "Records must not be used as the type of an attribute.")


class ForbiddenDictionaryTypeForAttributes(RuleBase):
    def validate(self, assert_, attribute):
        assert_(not attribute.idl_type.unwrap().is_dictionary,
                "Dictionaries must not be used as the type of an attribute.")


def register_rules(rule_store):
    rule_store.register(target.ATTRIBUTES,
                        ForbiddenSequenceTypeForAttributes())
    rule_store.register(target.ATTRIBUTES, ForbiddenRecordTypeForAttributes())
    rule_store.register(target.ATTRIBUTES,
                        ForbiddenDictionaryTypeForAttributes())

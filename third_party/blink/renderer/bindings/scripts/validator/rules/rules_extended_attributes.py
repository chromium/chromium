# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether the extended attributes of targets
violate the rules described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from . import supported_extended_attributes
from validator.framework import RuleBase
from validator.framework import target


class ExtendedAttributesOnNonType(RuleBase):
    def validate(self, assert_, target_object):
        for ext_attr in target_object.extended_attributes:
            supported_extended_attributes.validate(assert_, target_object,
                                                   ext_attr)


class ExtendedAttributesOnType(RuleBase):
    def validate(self, assert_, target_object):
        for ext_attr in target_object.effective_annotations:
            supported_extended_attributes.validate(assert_, target_object,
                                                   ext_attr)


def register_rules(rule_store):
    rule_store.register(target.OBJECTS_WITH_EXTENDED_ATTRIBUTES,
                        ExtendedAttributesOnNonType())
    rule_store.register(target.IDL_TYPES, ExtendedAttributesOnType())

# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Here are rule classes which validate whether the extended attributes of targets
violate the rules described in Web IDL https://webidl.spec.whatwg.org/.

Each rule class must inherit RuleBase.
"""

from validator.framework import target
from validator.framework import RuleBase

_web_idl_extended_attributes_applicable_to_types = [
    "AllowShared",
    "Clamp",
    "EnforceRange",
    "LegacyNullToEmptyString",
]
_blink_specific_extended_attributes_applicable_to_types = [
    "BufferSourceTypeNoSizeLimit",
    "FlexibleArrayBufferView",
    "StringContext",
    # "TreatNullAs" is the old version of "LegacyNullToEmptyString".
    "TreatNullAs",
]
_extended_attributes_applicable_to_types = (
    _web_idl_extended_attributes_applicable_to_types +
    _blink_specific_extended_attributes_applicable_to_types)


class ExtendedAttributesApplicableToTypes(RuleBase):
    def validate(self, assert_, target_object):
        web_idl_link = "https://webidl.spec.whatwg.org/#extended-attributes-applicable-to-types"
        for extended_attribute in target_object.extended_attributes.keys():
            assert_(
                extended_attribute not in
                _extended_attributes_applicable_to_types,
                ("Extended attribute '{}' is applicable to types, "
                 "but applied in the wrong context. See {}"),
                extended_attribute, web_idl_link)


class ExtendedAttributesApplicableToTypesForIdlType(RuleBase):
    def validate(self, assert_, target_object):
        web_idl_link = "https://webidl.spec.whatwg.org/#extended-attributes-applicable-to-types"
        for annotation in target_object.effective_annotations:
            assert_(annotation.key in _extended_attributes_applicable_to_types,
                    ("Extended attribute '{}' is not applicable to types, "
                     "but applied to a type. See {}"), annotation.key,
                    web_idl_link)


def register_rules(rule_store):
    rule_store.register(target.OBJECTS_WITH_EXTENDED_ATTRIBUTES,
                        ExtendedAttributesApplicableToTypes())
    rule_store.register(target.IDL_TYPES,
                        ExtendedAttributesApplicableToTypesForIdlType())

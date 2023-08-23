# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from validator.framework import RuleBase
from validator.framework import target


class ForbiddenObservableArrayTypeForArguments(RuleBase):
    def validate(self, assert_, argument):
        assert_(not argument.idl_type.unwrap().is_observable_array,
                "Observable arrays must not be used as argument types")


class ForbiddenObservableArrayInStaticAttribute(RuleBase):
    def validate(self, assert_, attribute):
        assert_(
            not (attribute.idl_type.unwrap().is_observable_array
                 and attribute.is_static),
            "Observable arrays must not be used in static attributes")


class ForbiddenObservableArrayElementTypes(RuleBase):
    def validate(self, assert_, observable_array):
        element_type = observable_array.element_type.unwrap()
        assert_(
            not (element_type.is_dictionary or element_type.is_sequence or
                 element_type.is_record or element_type.is_observable_array),
            ("An observable array's element type must not be dictionary, "
             "sequence, record or observable array."))


def register_rules(rule_store):
    rule_store.register(target.ARGUMENTS,
                        ForbiddenObservableArrayTypeForArguments())
    rule_store.register(target.ATTRIBUTES,
                        ForbiddenObservableArrayInStaticAttribute())
    rule_store.register(target.OBSERVABLE_ARRAYS,
                        ForbiddenObservableArrayElementTypes())

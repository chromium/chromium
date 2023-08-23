# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from validator.framework import RuleBase
from validator.framework import target


class IdlTypes(RuleBase):
    def __init__(self):
        self._assert = None

    def validate(self, assert_, idl_type):
        self._assert = assert_
        idl_type.apply_to_all_composing_elements(
            lambda idl_type: self.validate_composing_element(idl_type))
        self._assert = None

    def validate_composing_element(self, idl_type):
        assert_ = self._assert

        if idl_type.is_typedef:
            assert_(not idl_type.original_type.is_typedef,
                    "Typedef to another typedef is not allowed.")
        elif idl_type.is_nullable:
            inner_type = idl_type.inner_type
            assert_(not inner_type.is_any,
                    "Nullable type's inner type must not be 'any'.")
            assert_(not inner_type.is_promise,
                    "Nullable type's inner type must not be a promise type.")
            assert_(
                not inner_type.is_observable_array,
                "Nullable type's inner type must not be "
                "an observable array type.")
            assert_(
                not inner_type.is_nullable,
                "Nullable type's inner type must not be "
                "another nullable type.")
            assert_(
                not (inner_type.is_union
                     and inner_type.does_include_nullable_or_dict),
                "Nullable type's inner type must not be "
                "an union that includes a nullable type or "
                "a dictionary type.")


def register_rules(rule_store):
    rule_store.register(target.IDL_TYPES, IdlTypes())

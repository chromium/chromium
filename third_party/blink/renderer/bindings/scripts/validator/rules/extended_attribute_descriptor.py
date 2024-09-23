# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import enum

import web_idl


class ExtendedAttributeDescriptor(object):
    class Target(enum.Enum):
        ASYNC_ITERABLE = web_idl.AsyncIterable
        ATTRIBUTE = web_idl.Attribute
        CALLBACK_FUNCTION = web_idl.CallbackFunction
        CALLBACK_INTERFACE = web_idl.CallbackInterface
        CONSTANT = web_idl.Constant
        CONSTRUCTOR = web_idl.Constructor
        DICTIONARY = web_idl.Dictionary
        DICTIONARY_MEMBER = web_idl.DictionaryMember
        INTERFACE = web_idl.Interface
        ITERABLE = web_idl.Iterable
        LEGACY_WINDOW_ALIAS = web_idl.LegacyWindowAlias
        NAMESPACE = web_idl.Namespace
        OPERATION = web_idl.Operation
        TYPE = web_idl.IdlType

    class Form(enum.Enum):
        # https://webidl.spec.whatwg.org/#idl-extended-attributes
        NO_ARGS = enum.auto()  # [ExtAttr]
        IDENT = enum.auto()  # [ExtAttr=Value]
        IDENT_LIST = enum.auto()  # [ExtAttr=(Value1, ...)]
        ARG_LIST = enum.auto()  # [ExtAttr(V1L V1R, ...)]
        NAMED_ARG_LIST = enum.auto()  # [ExtAttr=Name(V1L V1R, ...)]

    def __init__(self,
                 name,
                 applicable_to=None,
                 forms=None,
                 values=None,
                 post_validate=None):
        assert isinstance(name, str)
        assert isinstance(applicable_to, list) and all(
            isinstance(target, ExtendedAttributeDescriptor.Target)
            for target in applicable_to)
        assert forms is None or isinstance(
            forms, ExtendedAttributeDescriptor.Form) or (isinstance(
                forms, list) and all(
                    isinstance(form, ExtendedAttributeDescriptor.Form)
                    for form in forms))
        assert values is None or (isinstance(values, list) and all(
            isinstance(value, str) for value in values))
        assert post_validate is None or callable(post_validate)

        self._name = name
        # self._applicable_to is a list of valid target object's types, e.g.
        # web_idl.Attribute, web_idl.Constant, etc.
        self._applicable_to = tuple(map(lambda e: e.value, applicable_to))
        # self._forms is a list of valid forms.
        if forms is None:
            self._forms = [ExtendedAttributeDescriptor.Form.NO_ARGS]
        elif not isinstance(forms, list):
            self._forms = [forms]
        else:
            self._forms = forms
        # self._values is a list of valid "ident" values
        if values is None:
            self._values = None
        else:
            assert (ExtendedAttributeDescriptor.Form.IDENT in self._forms or
                    ExtendedAttributeDescriptor.Form.IDENT_LIST in self._forms)
            self._values = values
        # self._post_validate is a callable or None.
        self._post_validate = post_validate

    @property
    def name(self):
        return self._name

    def validate(self, assert_, target_object, ext_attr):
        T = ExtendedAttributeDescriptor.Target
        F = ExtendedAttributeDescriptor.Form

        failure_count = [0]

        def _assert(condition, text, *args, **kwargs):
            if not condition:
                failure_count[0] = failure_count[0] + 1
                assert_(condition, text, *args, **kwargs)

        # applicable_to
        _assert(isinstance(target_object, self._applicable_to),
                "[{}] is not applicable to {}.", self._name,
                target_object.__class__.__name__)

        # forms
        if ext_attr.has_values:
            if not ext_attr.values:
                _assert(F.NO_ARGS in self._forms,
                        "[{}] needs an identifier or an argument list.",
                        self._name)
            elif F.IDENT_LIST in self._forms:
                pass
            elif F.IDENT in self._forms:
                _assert(
                    len(ext_attr.values) == 1,
                    "[{}] doesn't take an identifier list.", self._name)
            elif F.ARG_LIST in self._forms or F.NAMED_ARG_LIST in self._forms:
                _assert(False, "[{}] needs an argument list.", self._name)
            else:  # F.NO_ARGS only
                _assert(False, "[{}] doesn't take an identifier.", self._name)
        if ext_attr.has_arguments:
            _assert(
                F.ARG_LIST in self._forms or F.NAMED_ARG_LIST in self._forms,
                "[{}] doesn't take an argument list.", self._name)
        if ext_attr.has_name:
            _assert(F.NAMED_ARG_LIST in self._forms,
                    "[{}] doesn't take an named argument list.", self._name)

        # values
        if self._values:
            for value in ext_attr.values:
                _assert(value in self._values, "[{}={}] is not supported.",
                        self._name, value)

        # post_validate
        if self._post_validate:
            if failure_count[0] == 0:
                self._post_validate(assert_, target_object, ext_attr)

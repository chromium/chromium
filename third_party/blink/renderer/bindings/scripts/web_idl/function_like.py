# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .argument import Argument
from .composition_parts import WithIdentifier
from .idl_type import IdlType


class FunctionLike(WithIdentifier):
    class IR(WithIdentifier):
        def __init__(self, identifier, arguments, return_type,
                     is_static=False):
            assert isinstance(arguments, (list, tuple)) and all(
                isinstance(arg, Argument.IR) for arg in arguments)
            assert isinstance(return_type, IdlType)
            assert isinstance(is_static, bool)

            WithIdentifier.__init__(self, identifier)
            self.arguments = list(arguments)
            self.return_type = return_type
            self.is_static = is_static

    def __init__(self, ir):
        assert isinstance(ir, FunctionLike.IR)

        WithIdentifier.__init__(self, ir.identifier)
        self._overload_group = None
        self._arguments = tuple(
            [Argument(arg_ir, self) for arg_ir in ir.arguments])
        self._return_type = ir.return_type
        self._is_static = ir.is_static

    @property
    def overload_group(self):
        """Returns the OverloadGroup that this function belongs to."""
        return self._overload_group

    def set_overload_group(self, overload_group):
        assert isinstance(overload_group, OverloadGroup)
        assert self._overload_group is None
        assert self in overload_group
        self._overload_group = overload_group

    @property
    def overload_index(self):
        """Returns the index in the OverloadGroup."""
        return self.overload_group.index(self)

    @property
    def arguments(self):
        """Returns a list of arguments."""
        return self._arguments

    @property
    def return_type(self):
        """Returns the return type."""
        return self._return_type

    @property
    def is_static(self):
        """Returns True if this is a static function."""
        return self._is_static

    @property
    def is_variadic(self):
        """Returns True if this function takes variadic arguments."""
        return bool(self.arguments and self.arguments[-1].is_variadic)

    @property
    def num_of_required_arguments(self):
        """Returns the number of required arguments."""
        return len(
            list(
                filter(lambda arg: not (arg.is_optional or arg.is_variadic),
                       self.arguments)))


class OverloadGroup(WithIdentifier):
    class IR(WithIdentifier):
        def __init__(self, functions):
            assert isinstance(functions, (list, tuple))
            assert all(
                isinstance(function, FunctionLike.IR)
                for function in functions)
            assert len(set(
                [function.identifier for function in functions])) == 1
            assert len(set(
                [function.is_static for function in functions])) == 1

            WithIdentifier.__init__(self, functions[0].identifier)
            self.functions = list(functions)
            self.is_static = functions[0].is_static

        def __iter__(self):
            return iter(self.functions)

        def __len__(self):
            return len(self.functions)

    class EffectiveOverloadItem(object):
        """
        Represents an item in an effective overload set.
        https://webidl.spec.whatwg.org/#dfn-effective-overload-set
        """

        def __init__(self, function_like, type_list, opt_list):
            assert len(type_list) == len(opt_list)
            assert isinstance(function_like, FunctionLike)
            assert isinstance(type_list, (list, tuple))
            assert all(isinstance(idl_type, IdlType) for idl_type in type_list)
            assert isinstance(opt_list, (list, tuple))
            assert all(
                isinstance(optionality, IdlType.Optionality.Type)
                for optionality in opt_list)

            self._function_like = function_like
            self._type_list = tuple(type_list)
            self._opt_list = tuple(opt_list)

        @property
        def function_like(self):
            return self._function_like

        @property
        def type_list(self):
            return self._type_list

        @property
        def opt_list(self):
            return self._opt_list

    def __init__(self, functions):
        assert isinstance(functions, (list, tuple))
        assert all(
            isinstance(function, FunctionLike) for function in functions)
        assert len(set([function.identifier for function in functions])) == 1
        assert len(set([function.is_static for function in functions])) == 1

        WithIdentifier.__init__(self, functions[0].identifier)
        self._functions = tuple(functions)
        for function in self._functions:
            function.set_overload_group(self)
        self._is_static = functions[0].is_static

    def __getitem__(self, index):
        return self._functions[index]

    def __iter__(self):
        return iter(self._functions)

    def __len__(self):
        return len(self._functions)

    def index(self, value):
        return self._functions.index(value)

    @property
    def is_static(self):
        return self._is_static

    @property
    def min_num_of_required_arguments(self):
        """
        Returns the minimum number of required arguments of overloaded
        functions.
        """
        return min(map(lambda func: func.num_of_required_arguments, self))

    def effective_overload_set(self, argument_count=None):
        """
        Returns the effective overload set.
        https://webidl.spec.whatwg.org/#compute-the-effective-overload-set
        """
        assert argument_count is None or isinstance(argument_count, int)

        N = argument_count
        S = []
        F = self

        maxarg = max(map(lambda X: len(X.arguments), F))
        if N is None:
            arg_sizes = [len(X.arguments) for X in F if not X.is_variadic]
            N = 1 + (max(arg_sizes) if arg_sizes else 0)

        for X in F:
            n = len(X.arguments)

            S.append(
                OverloadGroup.EffectiveOverloadItem(
                    X, list(map(lambda arg: arg.idl_type, X.arguments)),
                    list(map(lambda arg: arg.optionality, X.arguments))))

            if X.is_variadic:
                for i in range(n, max(maxarg, N)):
                    t = list(map(lambda arg: arg.idl_type, X.arguments))
                    o = list(map(lambda arg: arg.optionality, X.arguments))
                    for _ in range(n, i + 1):
                        t.append(X.arguments[-1].idl_type)
                        o.append(X.arguments[-1].optionality)
                    S.append(OverloadGroup.EffectiveOverloadItem(X, t, o))

            t = list(map(lambda arg: arg.idl_type, X.arguments))
            o = list(map(lambda arg: arg.optionality, X.arguments))
            for i in range(n - 1, -1, -1):
                if X.arguments[i].optionality == IdlType.Optionality.REQUIRED:
                    break
                S.append(OverloadGroup.EffectiveOverloadItem(X, t[:i], o[:i]))

        return S

    @staticmethod
    def distinguishing_argument_index(items_of_effective_overload_set):
        """
        Returns the distinguishing argument index.
        https://webidl.spec.whatwg.org/#dfn-distinguishing-argument-index
        """
        items = items_of_effective_overload_set
        assert isinstance(items, (list, tuple))
        assert all(
            isinstance(item, OverloadGroup.EffectiveOverloadItem)
            for item in items)
        assert len(items) > 1

        for index in range(len(items[0].type_list)):
            # Assume that the given items are valid, and we only need to test
            # the two types.
            if OverloadGroup.are_distinguishable_types(
                    items[0].type_list[index], items[1].type_list[index]):
                return index
        assert False

    @staticmethod
    def are_distinguishable_types(idl_type1, idl_type2):
        """
        Returns True if the two given types are distinguishable.
        https://webidl.spec.whatwg.org/#dfn-distinguishable
        """
        assert isinstance(idl_type1, IdlType)
        assert isinstance(idl_type2, IdlType)

        # step 1. If one type includes a nullable type and the other type either
        #   includes a nullable type, is a union type with flattened member
        #   types including a dictionary type, or is a dictionary type, ...
        if ((idl_type1.does_include_nullable_type
             and idl_type2.does_include_nullable_or_dict)
                or (idl_type2.does_include_nullable_type
                    and idl_type1.does_include_nullable_or_dict)):
            return False

        type1 = idl_type1.unwrap()
        type2 = idl_type2.unwrap()

        # step 2. If both types are either a union type or nullable union type,
        #   ...
        if type1.is_union and type2.is_union:
            for member1 in type1.member_types:
                for member2 in type2.member_types:
                    if not OverloadGroup.are_distinguishable_types(
                            member1, member2):
                        return False
            return True

        # step 3. If one type is a union type or nullable union type, ...
        if type1.is_union or type2.is_union:
            union = type1 if type1.is_union else type2
            other = type2 if type1.is_union else type1
            for member in union.member_types:
                if not OverloadGroup.are_distinguishable_types(member, other):
                    return False
            return True

        # step 4. Consider the two "innermost" types ...
        def is_string_type(idl_type):
            return idl_type.is_string or idl_type.is_enumeration

        def is_interface_like(idl_type):
            return idl_type.is_interface or idl_type.is_buffer_source_type

        def is_dictionary_like(idl_type):
            return (idl_type.is_dictionary or idl_type.is_record
                    or idl_type.is_callback_interface)

        def is_sequence_like(idl_type):
            return idl_type.is_sequence or idl_type.is_frozen_array

        if not (type2.is_undefined or type2.is_boolean or type2.is_numeric
                or type2.is_bigint or type2.is_string or type2.is_object
                or type2.is_symbol or is_interface_like(type2)
                or type2.is_callback_function or is_dictionary_like(type2)
                or is_sequence_like(type2)):
            return False  # Out of the table

        if type1.is_undefined:
            return not (type2.is_undefined or is_dictionary_like(type2))
        if type1.is_boolean:
            return not type2.is_boolean
        if type1.is_numeric or type1.is_bigint:
            # The spec distinguishes numeric types from bigint, but there is no
            # good use case yet. Also note that it's quite confusing and
            # problematic to abuse this distinguishment. Thus, we don't
            # distinguish them for now.
            return not (type2.is_numeric or type2.is_bigint)
        if is_string_type(type1):
            return not is_string_type(type2)
        if type1.is_object:
            return (type2.is_undefined or type2.is_boolean or type2.is_numeric
                    or type2.is_bigint or is_string_type(type2)
                    or type2.is_symbol)
        if type1.is_symbol:
            return not type2.is_symbol
        if is_interface_like(type1):
            if type2.is_object:
                return False
            if not is_interface_like(type2):
                return True
            # Additional requirements: The two identified interface-like types
            # are not the same, and no single platform object implements both
            # interface-like types.
            if type1.is_buffer_source_type or type2.is_buffer_source_type:
                return type1.keyword_typename != type2.keyword_typename
            interface1 = type1.type_definition_object
            interface2 = type2.type_definition_object
            return not (
                interface1 in interface2.inclusive_inherited_interfaces
                or interface2 in interface1.inclusive_inherited_interfaces)
        if type1.is_callback_function:
            if type2.is_object or type2.is_callback_function:
                return False
            if not is_dictionary_like(type2):
                return True
            # Additional requirements: A callback function that does not have
            # [LegacyTreatNonObjectAsNull] extended attribute is
            # distinguishable from a type in the dictionary-like category.
            return ("LegacyTreatNonObjectAsNull"
                    not in type1.type_definition_object.extended_attributes)
        if is_dictionary_like(type1):
            if (type2.is_undefined or type2.is_object
                    or is_dictionary_like(type2)):
                return False
            if not type2.is_callback_function:
                return True
            # Additional requirements: A callback function that does not have
            # [LegacyTreatNonObjectAsNull] extended attribute is
            # distinguishable from a type in the dictionary-like category.
            return ("LegacyTreatNonObjectAsNull"
                    not in type2.type_definition_object.extended_attributes)
        if is_sequence_like(type1):
            return not (type2.is_object or is_sequence_like(type2))
        return False  # Out of the table

# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import itertools

from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Identifier
from .constructor import ConstructorGroup
from .database import Database
from .database import DatabaseBody
from .dictionary import Dictionary
from .enumeration import Enumeration
from .idl_type import IdlTypeFactory
from .interface import Interface
from .ir_map import IRMap
from .make_copy import make_copy
from .namespace import Namespace
from .operation import OperationGroup
from .reference import RefByIdFactory
from .typedef import Typedef
from .union import Union
from .user_defined_type import StubUserDefinedType
from .user_defined_type import UserDefinedType


class IdlCompiler(object):
    """
    Compiles IRs of Web IDL definitions into a database.

    IdlCompiler works very closely with IRs.  IdlCompiler resolves a lot of
    things such as; merge partial definitions, merge mixins into an interface,
    resolve inheritance, etc.  These tasks must be done in order, and it's
    represented with "(compilation) phase".

    IdlCompiler works proceeding one phase to next phase.  A basic strategy is
    like below.

    1. prev_phase = self._ir_map.current_phase, next_phase = prev_phase + 1
    2. for x = an IR in self._ir_map(phase=prev_phase)
    2.1. y = process_and_update(x.copy())
    2.2. self._ir_map(phase=next_phase).add(y)

    Note that an old IR for 'x' remains internally.  See IRMap for the details.
    """

    def __init__(self, ir_map, ref_to_idl_def_factory, idl_type_factory,
                 report_error):
        """
        Args:
            ir_map: IRMap filled with the initial IRs of IDL definitions.
            ref_to_idl_def_factory: RefByIdFactory that created all references
                to UserDefinedType.
            idl_type_factory: IdlTypeFactory that created all instances of
                IdlType.
            report_error: A callback that will be invoked when an error occurs
                due to inconsistent/invalid IDL definitions.  This callback
                takes an error message of type str and return value is not used.
                It's okay to terminate the program in this callback.
        """
        assert isinstance(ir_map, IRMap)
        assert isinstance(ref_to_idl_def_factory, RefByIdFactory)
        assert isinstance(idl_type_factory, IdlTypeFactory)
        assert callable(report_error)
        self._ir_map = ir_map
        self._ref_to_idl_def_factory = ref_to_idl_def_factory
        self._idl_type_factory = idl_type_factory
        self._report_error = report_error
        self._db = DatabaseBody()
        self._did_run = False  # Run only once.

    def build_database(self):
        assert not self._did_run
        self._did_run = True

        # Merge partial definitions.
        self._propagate_extattrs_per_idl_fragment()
        self._merge_partial_interface_likes()
        self._merge_partial_dictionaries()
        # Merge mixins.
        self._merge_interface_mixins()

        # Process inheritances.
        self._process_interface_inheritances()

        # Make groups of overloaded functions including inherited ones.
        self._group_overloaded_functions()
        self._calculate_group_exposure()

        # Updates on IRs are finished.  Create API objects.
        self._create_public_objects()

        # Resolve references.
        self._resolve_references_to_idl_def()
        self._resolve_references_to_idl_type()

        # Build union API objects.
        self._create_public_unions()

        return Database(self._db)

    def _propagate_extattrs_per_idl_fragment(self):
        def propagate_extattr(extattr_key_and_attr_name,
                              bag=None,
                              default_value=None,
                              only_to_members_of_partial_or_mixin=True,
                              ir=None):
            """
            Given |extattr_key| and |attr_name|, this function works like below.

              extattr = ir.extended_attributes.get(extattr_key)
              ir.exposure.attr_name(extattr's contents)          # [1]

            |bag| selects either of code_generator_info or exposure.  |apply_to|
            defined below performs the second line above [1].
            """
            extattr_key, attr_name = extattr_key_and_attr_name
            extattr = ir.extended_attributes.get(extattr_key)
            if extattr is None:
                return

            def apply_to(x):
                set_func = getattr(getattr(x, bag), attr_name)
                if extattr.has_values:
                    for value in extattr.values:
                        set_func(value)
                    if not extattr.values and default_value:
                        set_func(default_value)
                elif extattr.has_arguments:
                    for left, right in extattr.arguments:
                        set_func(left, right)
                else:
                    assert False

            apply_to(ir)

            if not hasattr(ir, 'iter_all_members'):
                return
            if (only_to_members_of_partial_or_mixin
                    and ((hasattr(ir, 'is_partial') and ir.is_partial) or
                         (hasattr(ir, 'is_mixin') and ir.is_mixin))):
                return
            for member in ir.iter_all_members():
                apply_to(member)

        def process_interface_like(ir):
            ir = make_copy(ir)
            self._ir_map.add(ir)

            propagate = functools.partial(propagate_extattr, ir=ir)
            propagate(('ImplementedAs', 'set_receiver_implemented_as'),
                      bag='code_generator_info',
                      only_to_members_of_partial_or_mixin=False)
            propagate_to_exposure(propagate)

            map(process_member_like, ir.iter_all_members())

        def process_member_like(ir):
            propagate = functools.partial(propagate_extattr, ir=ir)
            propagate(('ImplementedAs', 'set_property_implemented_as'),
                      bag='code_generator_info')
            propagate_to_exposure(propagate)

        def propagate_to_exposure(propagate):
            propagate = functools.partial(propagate, bag='exposure')
            propagate(('Exposed', 'add_global_name_and_feature'))
            propagate(('RuntimeEnabled', 'add_runtime_enabled_feature'))
            propagate(('ContextEnabled', 'add_context_enabled_feature'))
            propagate(('SecureContext', 'set_only_in_secure_contexts'),
                      default_value=True)

        old_irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.INTERFACE_MIXIN,
            IRMap.IR.Kind.DICTIONARY, IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_DICTIONARY)

        self._ir_map.move_to_new_phase()

        map(process_interface_like, old_irs)

    def _merge_partial_interface_likes(self):
        irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.INTERFACE,
                                        IRMap.IR.Kind.INTERFACE_MIXIN,
                                        IRMap.IR.Kind.NAMESPACE)
        partial_irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_NAMESPACE)

        self._ir_map.move_to_new_phase()

        ir_sets_to_merge = [(ir, [
            partial_ir for partial_ir in partial_irs
            if partial_ir.identifier == ir.identifier
        ]) for ir in irs]
        self._merge_interface_like_irs(ir_sets_to_merge)

    def _merge_partial_dictionaries(self):
        old_dictionaries = self._ir_map.find_by_kind(IRMap.IR.Kind.DICTIONARY)
        old_partial_dictionaries = self._ir_map.find_by_kind(
            IRMap.IR.Kind.PARTIAL_DICTIONARY)

        self._ir_map.move_to_new_phase()

        for identifier, old_dictionary in old_dictionaries.iteritems():
            new_dictionary = make_copy(old_dictionary)
            self._ir_map.add(new_dictionary)
            for partial_dictionary in old_partial_dictionaries.get(
                    identifier, []):
                new_dictionary.add_components(partial_dictionary.components)
                new_dictionary.debug_info.add_locations(
                    partial_dictionary.debug_info.all_locations)
                new_dictionary.own_members.extend(
                    make_copy(partial_dictionary.own_members))

    def _merge_interface_mixins(self):
        interfaces = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE)
        mixins = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE_MIXIN)
        includes = self._ir_map.find_by_kind(IRMap.IR.Kind.INCLUDES)

        ir_sets_to_merge = [(interface, [
            mixins[include.mixin_identifier]
            for include in includes.get(identifier, [])
        ]) for identifier, interface in interfaces.iteritems()]

        self._ir_map.move_to_new_phase()

        self._merge_interface_like_irs(ir_sets_to_merge)

    def _merge_interface_like_irs(self, old_irs_to_merge):
        for old_ir, irs_to_be_merged in old_irs_to_merge:
            new_ir = make_copy(old_ir)
            self._ir_map.add(new_ir)
            for ir in irs_to_be_merged:
                to_be_merged = make_copy(ir)
                new_ir.add_components(to_be_merged.components)
                new_ir.debug_info.add_locations(
                    to_be_merged.debug_info.all_locations)
                new_ir.attributes.extend(to_be_merged.attributes)
                new_ir.constants.extend(to_be_merged.constants)
                new_ir.operations.extend(to_be_merged.operations)

    def _process_interface_inheritances(self):
        def is_own_member(member):
            return 'Unforgeable' in member.extended_attributes

        def create_inheritance_stack(obj, table):
            if obj.inherited is None:
                return [obj]
            return [obj] + create_inheritance_stack(
                table[obj.inherited.identifier], table)

        old_interfaces = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        for old_interface in old_interfaces.itervalues():
            new_interface = make_copy(old_interface)
            self._ir_map.add(new_interface)
            inheritance_stack = create_inheritance_stack(
                old_interface, old_interfaces)
            for interface in inheritance_stack[1:]:
                new_interface.attributes.extend([
                    make_copy(attribute) for attribute in interface.attributes
                    if is_own_member(attribute)
                ])
                new_interface.operations.extend([
                    make_copy(operation) for operation in interface.operations
                    if is_own_member(operation)
                ])

    def _group_overloaded_functions(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            assert not old_ir.constructor_groups
            assert not old_ir.operation_groups
            new_ir = make_copy(old_ir)
            self._ir_map.add(new_ir)
            sort_key = lambda x: x.identifier
            new_ir.constructor_groups = [
                ConstructorGroup.IR(constructors=list(constructors))
                for identifier, constructors in itertools.groupby(
                    sorted(new_ir.constructors, key=sort_key), key=sort_key)
            ]
            new_ir.operation_groups = [
                OperationGroup.IR(operations=list(operations))
                for identifier, operations in itertools.groupby(
                    sorted(new_ir.operations, key=sort_key), key=sort_key)
                if identifier
            ]

    def _calculate_group_exposure(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = make_copy(old_ir)
            self._ir_map.add(new_ir)

            for group in new_ir.constructor_groups + new_ir.operation_groups:
                exposures = map(lambda overload: overload.exposure, group)

                # [Exposed]
                if any(not exposure.global_names_and_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed by default.
                else:
                    for exposure in exposures:
                        for entry in exposure.global_names_and_features:
                            group.exposure.add_global_name_and_feature(
                                entry.global_name, entry.feature)

                # [RuntimeEnabled]
                if any(not exposure.runtime_enabled_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed by default.
                else:
                    for exposure in exposures:
                        for name in exposure.runtime_enabled_features:
                            group.exposure.add_runtime_enabled_feature(name)

                # [ContextEnabled]
                if any(not exposure.context_enabled_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed by default.
                else:
                    for exposure in exposures:
                        for name in exposure.context_enabled_features:
                            group.exposure.add_context_enabled_feature(name)

                # [SecureContext]
                if any(exposure.only_in_secure_contexts is False
                       for exposure in exposures):
                    group.exposure.set_only_in_secure_contexts(False)
                elif all(exposure.only_in_secure_contexts is True
                         for exposure in exposures):
                    group.exposure.set_only_in_secure_contexts(True)
                else:
                    flag_names = tuple(
                        itertools.chain.from_iterable([
                            exposure.only_in_secure_contexts
                            for exposure in exposures
                            if exposure.only_in_secure_contexts is not True
                        ]))
                    group.exposure.set_only_in_secure_contexts(flag_names)

    def _create_public_objects(self):
        """Creates public representations of compiled objects."""
        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE):
            self._db.register(DatabaseBody.Kind.INTERFACE, Interface(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE_MIXIN):
            self._db.register(DatabaseBody.Kind.INTERFACE_MIXIN, Interface(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.NAMESPACE):
            self._db.register(DatabaseBody.Kind.NAMESPACE, Namespace(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.DICTIONARY):
            self._db.register(DatabaseBody.Kind.DICTIONARY, Dictionary(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.CALLBACK_INTERFACE):
            self._db.register(DatabaseBody.Kind.CALLBACK_INTERFACE,
                              CallbackInterface(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.CALLBACK_FUNCTION):
            self._db.register(DatabaseBody.Kind.CALLBACK_FUNCTION,
                              CallbackFunction(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.ENUMERATION):
            self._db.register(DatabaseBody.Kind.ENUMERATION, Enumeration(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.TYPEDEF):
            self._db.register(DatabaseBody.Kind.TYPEDEF, Typedef(ir))

    def _resolve_references_to_idl_def(self):
        def resolve(ref):
            try:
                idl_def = self._db.find_by_identifier(ref.identifier)
            except KeyError:
                self._report_error("{}: Unresolved reference to {}".format(
                    ref.ref_own_debug_info.location, ref.identifier))
                idl_def = StubUserDefinedType(ref.identifier)
            ref.set_target_object(idl_def)

        self._ref_to_idl_def_factory.for_each(resolve)

    def _resolve_references_to_idl_type(self):
        def resolve(ref):
            try:
                idl_def = self._db.find_by_identifier(ref.identifier)
            except KeyError:
                self._report_error("{}: Unresolved reference to {}".format(
                    ref.ref_own_debug_info.location, ref.identifier))
                idl_def = StubUserDefinedType(ref.identifier)
            if isinstance(idl_def, UserDefinedType):
                idl_type = self._idl_type_factory.definition_type(
                    user_defined_type=idl_def)
            elif isinstance(idl_def, Typedef):
                idl_type = self._idl_type_factory.typedef_type(typedef=idl_def)
            else:
                assert False
            ref.set_target_object(idl_type)

        self._idl_type_factory.for_each_reference(resolve)

    def _create_public_unions(self):
        all_union_types = []  # all instances of UnionType

        def collect_unions(idl_type):
            if idl_type.is_union:
                all_union_types.append(idl_type)

        self._idl_type_factory.for_each(collect_unions)

        def unique_key(union_type):
            """
            Returns an unique (but meaningless) key.  Returns the same key for
            the identical union types.
            """
            key_pieces = sorted([
                idl_type.syntactic_form
                for idl_type in union_type.flattened_member_types
            ])
            if union_type.does_include_nullable_type:
                key_pieces.append('type null')  # something unique
            return '|'.join(key_pieces)

        grouped_unions = {}  # {unique key: list of union types}
        for union_type in all_union_types:
            key = unique_key(union_type)
            grouped_unions.setdefault(key, []).append(union_type)

        grouped_typedefs = {}  # {unique key: list of typedefs to the union}
        all_typedefs = self._db.find_by_kind(DatabaseBody.Kind.TYPEDEF)
        for typedef in all_typedefs.itervalues():
            if not typedef.idl_type.is_union:
                continue
            key = unique_key(typedef.idl_type)
            grouped_typedefs.setdefault(key, []).append(typedef)

        for key, union_types in grouped_unions.iteritems():
            self._db.register(
                DatabaseBody.Kind.UNION,
                Union(
                    Identifier(key),  # dummy identifier
                    union_types=union_types,
                    typedef_backrefs=grouped_typedefs.get(key, [])))

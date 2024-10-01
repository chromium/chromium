# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import dataclasses
import functools
import itertools
import posixpath
import typing

from blinkbuild.name_style_converter import NameStyleConverter

from .async_iterator import AsyncIterator
from .argument import Argument
from .attribute import Attribute
from .callback_function import CallbackFunction
from .callback_interface import CallbackInterface
from .composition_parts import Identifier
from .constructor import Constructor
from .constructor import ConstructorGroup
from .database import Database
from .database import DatabaseBody
from .dictionary import Dictionary
from .enumeration import Enumeration
from .exposure import ExposureMutable
from .extended_attribute import ExtendedAttribute
from .extended_attribute import ExtendedAttributesMutable
from .function_like import FunctionLike
from .idl_type import _ArrayLikeType
from .idl_type import IdlType
from .idl_type import IdlTypeFactory
from .idl_type import NullableType
from .idl_type import PromiseType
from .idl_type import RecordType
from .idl_type import SimpleType
from .idl_type import ReferenceType
from .idl_type import UnionType
from .interface import Interface
from .interface import LegacyWindowAlias
from .ir_map import IRMap
from .make_copy import make_copy
from .namespace import Namespace
from .observable_array import ObservableArray
from .operation import Operation
from .operation import OperationGroup
from .reference import RefByIdFactory
from .sync_iterator import SyncIterator
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

        # Create AsyncIterator.IRs and SyncIterator.IRs, which are not
        # registered by _IRBuilder, prior to processing extended attributes,
        # etc.
        self._create_async_iterator_irs()
        self._create_sync_iterator_irs()

        # Merge partial definitions.
        self._record_defined_in_partial_and_mixin()
        self._propagate_extattrs_per_idl_fragment()
        self._determine_blink_headers()
        self._merge_partial_interface_likes()
        self._merge_partial_dictionaries()
        # Merge mixins.
        self._set_owner_mixin_of_mixin_members()
        self._merge_interface_mixins()

        # Process inheritances.
        self._process_interface_inheritances()

        # Assign v8::CppHeapPointerTag values
        self._assign_tags()

        # Temporary mitigation of misuse of [HTMLConstructor]
        # This should be removed once the IDL definitions get fixed.
        self._supplement_missing_html_constructor_operation()

        self._copy_legacy_factory_function_extattrs()

        # Make groups of overloaded functions including inherited ones.
        self._group_overloaded_functions()
        self._propagate_extattrs_to_overload_group()
        self._calculate_group_exposure()

        self._fill_exposed_constructs()

        self._sort_dictionary_members()

        self._calculate_dict_and_union_usage()

        # Updates on IRs are finished.  Create API objects.
        self._create_public_objects()

        # Resolve references.
        self._resolve_references_to_idl_def()
        self._resolve_references_to_idl_type()

        # Build union API objects.
        self._create_public_unions()

        # Build observable array API objects.
        self._create_public_observable_arrays()

        return Database(self._db)

    def _maybe_make_copy(self, ir):
        # You can make this function return make_copy(ir) for debugging
        # purpose, etc.
        return ir  # Skip copying as an optimization.

    def _create_async_iterator_irs(self):
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            if new_ir.async_iterable is None:
                continue

            assert new_ir.async_iterator is None
            iterable = new_ir.async_iterable
            component = new_ir.components[0]
            operations = []
            # 'next' and 'return' properties are defined at:
            # https://webidl.spec.whatwg.org/#es-asynchronous-iterator-prototype-object
            operations.append(
                Operation.IR(
                    identifier=Identifier('next'),
                    arguments=[],
                    # The return type is a promise type resolving to an
                    # iterator result.
                    # https://webidl.spec.whatwg.org/#iterator-result
                    return_type=self._idl_type_factory.promise_type(
                        result_type=self._idl_type_factory.simple_type(
                            'object'),
                        extended_attributes=ExtendedAttributesMutable([
                            ExtendedAttribute(
                                key='IDLTypeImplementedAsV8Promise'),
                        ])),
                    extended_attributes=ExtendedAttributesMutable([
                        ExtendedAttribute(key="CallWith",
                                          values="ScriptState"),
                        ExtendedAttribute(key="RaisesException"),
                    ]),
                    component=component))
            # Define the 'return' property if and only if an asynchronous
            # iterator return algorithm is defined for the interface.
            if ("HasAsyncIteratorReturnAlgorithm"
                    in iterable.extended_attributes):
                operations.append(
                    Operation.IR(
                        identifier=Identifier('return'),
                        # Can be called without arguments (e.g.
                        # AsyncIteratorClose()) or with one argument (e.g.
                        # yield*).
                        # https://tc39.es/ecma262/#sec-asynciteratorclose
                        # https://tc39.es/ecma262/#sec-generator-function-definitions-runtime-semantics-evaluation
                        arguments=[
                            Argument.IR(
                                identifier=Identifier('value'),
                                index=0,
                                idl_type=self._idl_type_factory.simple_type(
                                    'any', is_optional=True))
                        ],
                        # The return type is a promise type resolving to an
                        # iterator result.
                        # https://webidl.spec.whatwg.org/#iterator-result
                        return_type=self._idl_type_factory.promise_type(
                            result_type=self._idl_type_factory.simple_type(
                                'object'),
                            extended_attributes=ExtendedAttributesMutable([
                                ExtendedAttribute(
                                    key='IDLTypeImplementedAsV8Promise'),
                            ])),
                        extended_attributes=ExtendedAttributesMutable([
                            ExtendedAttribute(key="CallWith",
                                              values="ScriptState"),
                            ExtendedAttribute(key="RaisesException"),
                            ExtendedAttribute(key="ImplementedAs",
                                              values="returnForBinding"),
                        ]),
                        component=component))

            iterator_ir = AsyncIterator.IR(
                interface=self._ref_to_idl_def_factory.create(
                    new_ir.identifier),
                component=component,
                key_type=iterable.key_type,
                value_type=iterable.value_type,
                operations=operations)
            iterator_ir.code_generator_info.set_for_testing(
                new_ir.code_generator_info.for_testing)
            iterator_ir.debug_info.add_locations(
                iterable.debug_info.all_locations)

            self._ir_map.register(iterator_ir)

            new_ir.async_iterator = self._ref_to_idl_def_factory.create(
                iterator_ir.identifier)

    def _create_sync_iterator_irs(self):
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            if not ((new_ir.iterable and new_ir.iterable.key_type)
                    or new_ir.maplike or new_ir.setlike):
                continue

            assert new_ir.sync_iterator is None
            iterable = (new_ir.iterable or new_ir.maplike or new_ir.setlike)
            component = new_ir.components[0]
            operations = []
            # 'next' property is defined at:
            # https://webidl.spec.whatwg.org/#es-iterator-prototype-object
            operations.append(
                Operation.IR(
                    identifier=Identifier('next'),
                    arguments=[],
                    # The return value is an iterator result.
                    # https://webidl.spec.whatwg.org/#iterator-result
                    return_type=self._idl_type_factory.simple_type('object'),
                    extended_attributes=ExtendedAttributesMutable([
                        ExtendedAttribute(key="CallWith",
                                          values="ScriptState"),
                        ExtendedAttribute(key="RaisesException"),
                    ]),
                    component=component))

            iterator_ir = SyncIterator.IR(
                interface=self._ref_to_idl_def_factory.create(
                    new_ir.identifier),
                component=component,
                key_type=iterable.key_type,
                value_type=iterable.value_type,
                operations=operations)
            iterator_ir.code_generator_info.set_for_testing(
                new_ir.code_generator_info.for_testing)
            iterator_ir.debug_info.add_locations(
                iterable.debug_info.all_locations)

            self._ir_map.register(iterator_ir)

            new_ir.sync_iterator = self._ref_to_idl_def_factory.create(
                iterator_ir.identifier)

    def _record_defined_in_partial_and_mixin(self):
        old_irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.DICTIONARY, IRMap.IR.Kind.INTERFACE,
            IRMap.IR.Kind.INTERFACE_MIXIN, IRMap.IR.Kind.NAMESPACE,
            IRMap.IR.Kind.PARTIAL_DICTIONARY, IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_NAMESPACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)
            is_partial = False
            if hasattr(new_ir, 'is_partial') and new_ir.is_partial:
                is_partial = True
            for member in new_ir.iter_all_members():
                member.code_generator_info.set_defined_in_partial(is_partial)

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
                    and not ((hasattr(ir, 'is_partial') and ir.is_partial) or
                             (hasattr(ir, 'is_mixin') and ir.is_mixin))):
                return
            for member in ir.iter_all_members():
                apply_to(member)

        def process_interface_like(ir):
            propagate = functools.partial(propagate_extattr, ir=ir)
            propagate(('ImplementedAs', 'set_receiver_implemented_as'),
                      bag='code_generator_info',
                      only_to_members_of_partial_or_mixin=False)
            propagate_to_exposure(propagate)

            list(map(process_member_like, ir.iter_all_members()))

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
            propagate(('CrossOriginIsolated', 'set_only_in_coi_contexts'),
                      default_value=True)
            propagate(('CrossOriginIsolatedOrRuntimeEnabled',
                       'add_only_in_coi_contexts_or_runtime_enabled_feature'))
            propagate(('InjectionMitigated',
                       'set_only_in_injection_mitigated_contexts'),
                      default_value=True)
            propagate(('IsolatedContext', 'set_only_in_isolated_contexts'),
                      default_value=True)
            propagate(('SecureContext', 'set_only_in_secure_contexts'),
                      default_value=True)

        old_irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.ASYNC_ITERATOR, IRMap.IR.Kind.CALLBACK_INTERFACE,
            IRMap.IR.Kind.DICTIONARY, IRMap.IR.Kind.INTERFACE,
            IRMap.IR.Kind.INTERFACE_MIXIN, IRMap.IR.Kind.NAMESPACE,
            IRMap.IR.Kind.PARTIAL_DICTIONARY, IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_NAMESPACE, IRMap.IR.Kind.SYNC_ITERATOR)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            process_interface_like(new_ir)

            collection_like = (getattr(new_ir, 'async_iterable', None)
                               or getattr(new_ir, 'iterable', None))
            if collection_like:
                propagate = functools.partial(propagate_extattr,
                                              ir=collection_like)
                propagate_to_exposure(propagate)

    def _determine_blink_headers(self):
        irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.INTERFACE_MIXIN,
            IRMap.IR.Kind.NAMESPACE, IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_NAMESPACE)

        self._ir_map.move_to_new_phase()

        for old_ir in irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            if new_ir.is_mixin and not new_ir.is_partial:
                continue

            basepath, _ = posixpath.splitext(
                new_ir.debug_info.location.filepath)
            dirpath, filename = posixpath.split(basepath)
            impl_class = new_ir.extended_attributes.value_of('ImplementedAs')
            if impl_class:
                filename = NameStyleConverter(impl_class).to_snake_case()
            header = posixpath.join(dirpath,
                                    posixpath.extsep.join([filename, 'h']))
            new_ir.code_generator_info.set_blink_headers([header])

    def _check_existence_of_non_partials(self, non_partial_kind, partial_kind):
        non_partials = self._ir_map.find_by_kind(non_partial_kind)
        partials = self._ir_map.find_by_kind(partial_kind)
        for identifier, partial_irs in partials.items():
            if not non_partials.get(identifier):
                locations = ''.join(
                    map(lambda ir: '  {}\n'.format(ir.debug_info.location),
                        partial_irs))
                raise ValueError(
                    '{} {} is defined without a non-partial definition.\n'
                    '{}'.format(partial_irs[0].kind, identifier, locations))

    def _merge_partial_interface_likes(self):
        self._check_existence_of_non_partials(IRMap.IR.Kind.INTERFACE,
                                              IRMap.IR.Kind.PARTIAL_INTERFACE)
        self._check_existence_of_non_partials(
            IRMap.IR.Kind.INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN)
        self._check_existence_of_non_partials(IRMap.IR.Kind.NAMESPACE,
                                              IRMap.IR.Kind.PARTIAL_NAMESPACE)

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
        self._check_existence_of_non_partials(IRMap.IR.Kind.DICTIONARY,
                                              IRMap.IR.Kind.PARTIAL_DICTIONARY)

        old_dictionaries = self._ir_map.find_by_kind(IRMap.IR.Kind.DICTIONARY)
        old_partial_dictionaries = self._ir_map.find_by_kind(
            IRMap.IR.Kind.PARTIAL_DICTIONARY)

        self._ir_map.move_to_new_phase()

        for identifier, old_dictionary in old_dictionaries.items():
            new_dictionary = make_copy(old_dictionary)
            self._ir_map.add(new_dictionary)
            for partial_dictionary in old_partial_dictionaries.get(
                    identifier, []):
                new_dictionary.add_components(partial_dictionary.components)
                new_dictionary.debug_info.add_locations(
                    partial_dictionary.debug_info.all_locations)
                new_dictionary.own_members.extend(
                    make_copy(partial_dictionary.own_members))

    def _set_owner_mixin_of_mixin_members(self):
        mixins = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE_MIXIN)

        self._ir_map.move_to_new_phase()

        for old_ir in mixins:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)
            ref_to_mixin = self._ref_to_idl_def_factory.create(
                new_ir.identifier)
            for member in new_ir.iter_all_members():
                member.set_owner_mixin(ref_to_mixin)

    def _merge_interface_mixins(self):
        interfaces = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE)
        mixins = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE_MIXIN)
        includes = self._ir_map.find_by_kind(IRMap.IR.Kind.INCLUDES)

        ir_sets_to_merge = [(interface, [
            mixins[include.mixin_identifier]
            for include in includes.get(identifier, [])
        ]) for identifier, interface in interfaces.items()]

        self._ir_map.move_to_new_phase()

        self._merge_interface_like_irs(ir_sets_to_merge)

    def _merge_interface_like_irs(self, old_irs_to_merge):
        for old_ir, irs_to_be_merged in old_irs_to_merge:
            new_ir = make_copy(old_ir)
            self._ir_map.add(new_ir)
            for ir in irs_to_be_merged:
                to_be_merged = make_copy(ir)
                if new_ir.is_mixin == to_be_merged.is_mixin:
                    new_ir.add_components(to_be_merged.components)
                new_ir.debug_info.add_locations(
                    to_be_merged.debug_info.all_locations)
                new_ir.attributes.extend(to_be_merged.attributes)
                new_ir.constants.extend(to_be_merged.constants)
                new_ir.operations.extend(to_be_merged.operations)

                new_ir_headers = new_ir.code_generator_info.blink_headers
                to_be_merged_headers = (
                    to_be_merged.code_generator_info.blink_headers)
                if to_be_merged_headers is not None:
                    if new_ir_headers is None:
                        new_ir.code_generator_info.set_blink_headers(
                            to_be_merged_headers)
                    else:
                        new_ir_headers.extend(to_be_merged_headers)

    def _process_interface_inheritances(self):
        def create_inheritance_chain(obj, table):
            if obj.inherited is None:
                return [obj]
            return [obj] + create_inheritance_chain(
                table[obj.inherited.identifier], table)

        inherited_ext_attrs = (
            # (IDL extended attribute to be inherited,
            #  CodeGeneratorInfoMutable's set function)
            ('ActiveScriptWrappable', 'set_is_active_script_wrappable'),
            ('LegacyUnenumerableNamedProperties',
             'set_is_legacy_unenumerable_named_properties'),
        )

        def is_own_member(member):
            return 'LegacyUnforgeable' in member.extended_attributes

        old_interfaces = self._ir_map.find_by_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        identifier_to_subclass_set = {}
        identifier_to_direct_subclass_set = {}

        for old_interface in old_interfaces.values():
            new_interface = make_copy(old_interface)
            self._ir_map.add(new_interface)
            inheritance_chain = create_inheritance_chain(
                old_interface, old_interfaces)

            for interface in inheritance_chain:
                for ext_attr, set_func in inherited_ext_attrs:
                    if ext_attr in interface.extended_attributes:
                        getattr(new_interface.code_generator_info,
                                set_func)(True)

            for interface in inheritance_chain[1:]:
                new_interface.attributes.extend([
                    make_copy(attribute) for attribute in interface.attributes
                    if is_own_member(attribute)
                ])
                new_interface.operations.extend([
                    make_copy(operation) for operation in interface.operations
                    if is_own_member(operation)
                ])

                identifier_to_subclass_set.setdefault(
                    interface.identifier, set()).add(new_interface.identifier)
                if new_interface.inherited.identifier == interface.identifier:
                    identifier_to_direct_subclass_set.setdefault(
                        interface.identifier, set()).add(new_interface)


        for new_interface in self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE):
            assert not new_interface.subclasses
            assert not new_interface.direct_subclasses
            subclass_set = identifier_to_subclass_set.get(
                new_interface.identifier, set())
            new_interface.subclasses = list(
                map(lambda id_: self._ref_to_idl_def_factory.create(id_),
                    sorted(subclass_set)))
            direct_subclass_set = identifier_to_direct_subclass_set.get(
                new_interface.identifier, set())
            new_interface.direct_subclasses = sorted(
                direct_subclass_set, key=lambda subclass: subclass.identifier)

    def _supplement_missing_html_constructor_operation(self):
        # Temporary mitigation of misuse of [HTMLConstructor]
        # https://html.spec.whatwg.org/C/#htmlconstructor
        # [HTMLConstructor] must be applied to only the single constructor
        # operation, but it's now applied to interfaces without a constructor
        # operation declaration.
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            if not (not new_ir.constructors
                    and "HTMLConstructor" in new_ir.extended_attributes):
                continue

            html_constructor = Constructor.IR(
                identifier=None,
                arguments=[],
                return_type=self._idl_type_factory.reference_type(
                    new_ir.identifier),
                extended_attributes=ExtendedAttributesMutable(
                    [ExtendedAttribute(key="HTMLConstructor")]),
                component=new_ir.components[0],
                debug_info=new_ir.debug_info)
            new_ir.constructors.append(html_constructor)

    def _copy_legacy_factory_function_extattrs(self):
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        def copy_extattrs(ext_attrs, ir):
            if 'LegacyFactoryFunction_CallWith' in ext_attrs:
                ir.extended_attributes.append(
                    ExtendedAttribute(key='CallWith',
                                      values=ext_attrs.values_of(
                                          'LegacyFactoryFunction_CallWith')))
            if 'LegacyFactoryFunction_RaisesException' in ext_attrs:
                ir.extended_attributes.append(
                    ExtendedAttribute(key='RaisesException'))

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)
            for legacy_factory_function_ir in new_ir.legacy_factory_functions:
                copy_extattrs(new_ir.extended_attributes,
                              legacy_factory_function_ir)

    def _group_overloaded_functions(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.ASYNC_ITERATOR,
                                            IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE,
                                            IRMap.IR.Kind.SYNC_ITERATOR)

        self._ir_map.move_to_new_phase()

        def make_groups(group_ir_class, operations):
            sort_key = lambda x: (x.is_static, x.identifier)
            return [
                group_ir_class(list(operations_in_group))
                for key, operations_in_group in itertools.groupby(
                    sorted(operations, key=sort_key), key=sort_key)
                if key[1]  # This is the operation identifier.
            ]

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            assert not new_ir.constructor_groups
            assert not new_ir.legacy_factory_function_groups
            assert not new_ir.operation_groups
            new_ir.constructor_groups = make_groups(ConstructorGroup.IR,
                                                    new_ir.constructors)
            new_ir.legacy_factory_function_groups = make_groups(
                ConstructorGroup.IR, new_ir.legacy_factory_functions)
            new_ir.operation_groups = make_groups(OperationGroup.IR,
                                                  new_ir.operations)

            if not isinstance(new_ir, Interface.IR):
                continue

            for item in (new_ir.async_iterable, new_ir.iterable,
                         new_ir.maplike, new_ir.setlike):
                if item:
                    assert not item.operation_groups
                    item.operation_groups = make_groups(
                        OperationGroup.IR, item.operations)

    def _propagate_extattrs_to_overload_group(self):
        ANY_OF = ('CrossOrigin', 'CrossOriginIsolated', 'InjectionMitigated',
                  'IsolatedContext', 'LegacyLenientThis', 'LegacyUnforgeable',
                  'NotEnumerable', 'PerWorldBindings', 'SecureContext',
                  'Unscopable')

        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.ASYNC_ITERATOR,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE,
                                            IRMap.IR.Kind.SYNC_ITERATOR)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            for group in new_ir.iter_all_overload_groups():
                for key in ANY_OF:
                    if any(key in overload.extended_attributes
                           for overload in group):
                        group.extended_attributes.append(
                            ExtendedAttribute(key=key))

                # [Affects=] must be consistent among overloaded operations.
                affects_values = set()
                for overload in group:
                    affects_values.add(
                        overload.extended_attributes.value_of('Affects'))
                assert len(affects_values) == 1, (
                    "Overloaded operations have inconsistent extended "
                    "attributes of [Affects]. {}.{}".format(
                        new_ir.identifier, group.identifier))
                affects_value = affects_values.pop()
                if affects_value:
                    group.extended_attributes.append(
                        ExtendedAttribute(key='Affects', values=affects_value))

                # Check that overloads with the same number of parameters have
                # set the [NoAllocDirectCall] attribute consistently.
                nadc_set = set()
                no_nadc_set = set()
                for overload in group:

                    set_to_update = nadc_set
                    if "NoAllocDirectCall" not in overload.extended_attributes:
                        set_to_update = no_nadc_set

                    for argument in reversed(overload.arguments):
                        set_to_update.add(argument.index + 1)
                        if not argument.idl_type.is_optional:
                            break
                    else:
                        set_to_update.add(0)
                assert nadc_set.isdisjoint(no_nadc_set), (
                    "Overloaded operations with same parameter count "
                    "have inconsistent extended attributes of "
                    "[NoAllocDirectCall]. {}.{}".format(
                        new_ir.identifier, group.identifier))

                if len(nadc_set) > 0:
                    group.extended_attributes.append(
                        ExtendedAttribute(key='NoAllocDirectCall'))

    def _calculate_group_exposure(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.ASYNC_ITERATOR,
                                            IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE,
                                            IRMap.IR.Kind.SYNC_ITERATOR)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            for group in new_ir.iter_all_overload_groups():
                exposures = list(map(lambda overload: overload.exposure,
                                     group))

                # [Exposed]
                if any(not exposure.global_names_and_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed.
                else:
                    for exposure in exposures:
                        for entry in exposure.global_names_and_features:
                            group.exposure.add_global_name_and_feature(
                                entry.global_name, entry.feature)


                # [RuntimeEnabled]
                if any(not exposure.runtime_enabled_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed.
                else:
                    for exposure in exposures:
                        for name in exposure.runtime_enabled_features:
                            group.exposure.add_runtime_enabled_feature(name)

                # [ContextEnabled]
                if any(not exposure.context_enabled_features
                       for exposure in exposures):
                    pass  # Unconditionally exposed.
                else:
                    for exposure in exposures:
                        for name in exposure.context_enabled_features:
                            group.exposure.add_context_enabled_feature(name)

                # [CrossOriginIsolated]
                if any(not exposure.only_in_coi_contexts
                       for exposure in exposures):
                    pass  # Exposed by default.
                else:
                    group.exposure.set_only_in_coi_contexts(True)

                # [CrossOriginIsolatedOrRuntimeEnabled]
                features = set()
                for exposure in exposures:
                    for feature in (
                            exposure.
                            only_in_coi_contexts_or_runtime_enabled_features):
                        features.add(feature)
                for feature in sorted(features):
                    (group.exposure.
                     add_only_in_coi_contexts_or_runtime_enabled_feature
                     )(feature)

                # [InjectionMitigated]
                if any(not exposure.only_in_injection_mitigated_contexts
                       for exposure in exposures):
                    pass  # Exposed by default.
                else:
                    group.exposure.set_only_in_injection_mitigated_contexts(
                        True)

                # [IsolatedContext]
                if any(not exposure.only_in_isolated_contexts
                       for exposure in exposures):
                    pass  # Exposed by default.
                else:
                    group.exposure.set_only_in_isolated_contexts(True)

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

    def _fill_exposed_constructs(self):
        old_callback_interfaces = self._ir_map.irs_of_kind(
            IRMap.IR.Kind.CALLBACK_INTERFACE)
        old_interfaces = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)
        old_namespaces = self._ir_map.irs_of_kind(IRMap.IR.Kind.NAMESPACE)

        def make_legacy_window_alias(ir):
            ext_attrs = ir.extended_attributes
            identifier = Identifier(ext_attrs.value_of('LegacyWindowAlias'))
            original = self._ref_to_idl_def_factory.create(ir.identifier)
            extended_attributes = ExtendedAttributesMutable()
            exposure = ExposureMutable()
            if 'LegacyWindowAlias_Measure' in ext_attrs:
                extended_attributes.append(
                    ExtendedAttribute(
                        key='Measure',
                        values=ext_attrs.value_of(
                            'LegacyWindowAlias_Measure')))
            if 'LegacyWindowAlias_MeasureAs' in ext_attrs:
                extended_attributes.append(
                    ExtendedAttribute(key='MeasureAs',
                                      values=ext_attrs.value_of(
                                          'LegacyWindowAlias_MeasureAs')))
            if 'LegacyWindowAlias_RuntimeEnabled' in ext_attrs:
                feature_name = ext_attrs.value_of(
                    'LegacyWindowAlias_RuntimeEnabled')
                extended_attributes.append(
                    ExtendedAttribute(
                        key='RuntimeEnabled', values=feature_name))
                exposure.add_runtime_enabled_feature(feature_name)
            return LegacyWindowAlias(
                identifier=identifier,
                original=original,
                extended_attributes=extended_attributes,
                exposure=exposure)

        exposed_map = {}  # global name: [construct's identifier...]
        legacy_window_aliases = []
        for ir in itertools.chain(old_callback_interfaces, old_interfaces,
                                  old_namespaces):
            for pair in ir.exposure.global_names_and_features:
                exposed_map.setdefault(pair.global_name,
                                       []).append(ir.identifier)
            if 'LegacyWindowAlias' in ir.extended_attributes:
                legacy_window_aliases.append(make_legacy_window_alias(ir))

        self._ir_map.move_to_new_phase()

        for old_ir in old_interfaces:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            assert not new_ir.exposed_constructs
            # Not only [Global] but also [TargetOfExposed] will expose IDL
            # constructs with [Exposed].
            global_names = new_ir.extended_attributes.values_of('Global')
            toe_names = new_ir.extended_attributes.values_of('TargetOfExposed')
            if not (global_names or toe_names):
                continue
            constructs = set()
            for global_name in global_names:
                constructs.update(exposed_map.get(global_name, []))
            if global_names:
                # If it's a global object, then expose the constructs with the
                # wildcard exposure ([Exposed=*]).
                constructs.update(exposed_map.get('*', []))
            for toe_name in toe_names:
                constructs.update(exposed_map.get(toe_name, []))
            new_ir.exposed_constructs = list(
                map(self._ref_to_idl_def_factory.create, sorted(constructs)))

            assert not new_ir.legacy_window_aliases
            if new_ir.identifier != 'Window':
                continue
            new_ir.legacy_window_aliases = sorted(
                legacy_window_aliases, key=lambda x: x.identifier)

    def _sort_dictionary_members(self):
        """Sorts dictionary members in alphabetical order."""
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.DICTIONARY)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            new_ir.own_members.sort(key=lambda x: x.identifier)

    def _calculate_dict_and_union_usage(self):
        """Calculate what dictionaries and unions are used for input or output, so that
           unnecessary methods don't have to be generated.
        """

        typedefs = self._ir_map.find_by_kind(IRMap.IR.Kind.TYPEDEF)
        dicts = self._ir_map.find_by_kind(IRMap.IR.Kind.DICTIONARY)

        class UsageSet:

            def __init__(self):
                self.dicts = set()
                self.unions = set()

        inputs = UsageSet()
        outputs = UsageSet()

        def unwrap(idl_type):
            while True:
                if isinstance(idl_type, ReferenceType):
                    if typedef := typedefs.get(idl_type.identifier):
                        idl_type = typedef.idl_type
                    else:
                        return idl_type
                elif isinstance(idl_type, _ArrayLikeType):
                    idl_type = idl_type.element_type
                elif isinstance(idl_type, NullableType):
                    idl_type = idl_type.inner_type
                else:
                    return idl_type

        def visit_dict(dict_ir, target_set):
            assert isinstance(dict_ir, Dictionary.IR)
            if "ConvertibleToObject" in dict_ir.extended_attributes and target_set != outputs:
                visit_dict(dict_ir, outputs)
            if dict_ir.identifier in target_set.dicts:
                return
            target_set.dicts.add(dict_ir.identifier)
            if dict_ir.inherited:
                visit_dict(dicts.get(dict_ir.inherited.identifier), target_set)
            for member in dict_ir.own_members:
                visit_type(member.idl_type, target_set)

        def visit_type(idl_type, target_set):
            idl_type = unwrap(idl_type)
            if isinstance(idl_type, ReferenceType):
                if dict := dicts.get(idl_type.identifier):
                    visit_dict(dict, target_set)
            elif isinstance(idl_type, UnionType):
                if idl_type in target_set.unions:
                    return
                target_set.unions.add(idl_type)
                if "ConvertibleToObject" in idl_type.extended_attributes:
                    visit_type(idl_type, outputs)
                for member_type in idl_type.flattened_member_types:
                    visit_type(member_type, target_set)
            elif isinstance(idl_type, RecordType):
                visit_type(idl_type.value_type, target_set)
            elif isinstance(idl_type, PromiseType):
                visit_type(idl_type.result_type, target_set)
                # Because of how we convert Promise<> input parameters, we need
                # to be able to covert the result type back to V8.
                visit_type(idl_type.result_type, outputs)
            else:
                assert isinstance(idl_type, SimpleType), type(idl_type)

        def visit_func(function_like, ret_set, args_set):
            assert isinstance(function_like, FunctionLike.IR)
            visit_type(function_like.return_type, ret_set)
            for arg in function_like.arguments:
                visit_type(arg.idl_type, args_set)

        for interface in self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE):
            for op in interface.constructors:
                visit_func(op, outputs, inputs)
            for op in interface.operations:
                visit_func(op, outputs, inputs)
            if interface.async_iterable:
                for arg in interface.async_iterable.arguments:
                    visit_type(arg.idl_type, inputs)
            for attr in interface.attributes:
                visit_type(attr.idl_type, inputs)
                visit_type(attr.idl_type, outputs)

        for interface in self._ir_map.irs_of_kind(
                IRMap.IR.Kind.CALLBACK_INTERFACE):
            for op in interface.operations:
                visit_func(op, inputs, outputs)

        for cb in self._ir_map.irs_of_kind(IRMap.IR.Kind.CALLBACK_FUNCTION):
            visit_func(cb, inputs, outputs)

        # Dirty hack for internally used dictionaries -- if a dictionary
        # appears unused, presume it's used for output for now.
        for dict_id, dict in dicts.items():
            if dict_id not in inputs.dicts and dict_id not in outputs.dicts:
                visit_dict(dict, outputs)

        for dict in dicts.values():
            if dict.identifier in inputs.dicts:
                dict.add_usage(Dictionary.Usage.INPUT)
            if dict.identifier in outputs.dicts:
                dict.add_usage(Dictionary.Usage.OUTPUT)

        for u in inputs.unions:
            u.add_usage(UnionType.Usage.INPUT)
        for u in outputs.unions:
            u.add_usage(UnionType.Usage.OUTPUT)

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

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.ASYNC_ITERATOR):
            self._db.register(DatabaseBody.Kind.ASYNC_ITERATOR,
                              AsyncIterator(ir))

        for ir in self._ir_map.irs_of_kind(IRMap.IR.Kind.SYNC_ITERATOR):
            self._db.register(DatabaseBody.Kind.SYNC_ITERATOR,
                              SyncIterator(ir))

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
                    reference_type=ref, user_defined_type=idl_def)
            elif isinstance(idl_def, Typedef):
                idl_type = self._idl_type_factory.typedef_type(
                    reference_type=ref, typedef=idl_def)
            else:
                assert False
            ref.set_target_object(idl_type)

        self._idl_type_factory.for_each_reference(resolve)

    def _create_public_unions(self):
        all_union_types = []  # all instances of UnionType

        def collect_unions(idl_type):
            if idl_type.is_union and not idl_type.is_phantom:
                all_union_types.append(idl_type)

        self._idl_type_factory.for_each(collect_unions)

        grouped_unions = {}  # {unique token: list of union types}
        for union_type in all_union_types:
            token = Union.unique_token(union_type)
            grouped_unions.setdefault(token, []).append(union_type)

        irs = {}  # {token: Union.IR}
        for token, union_types in grouped_unions.items():
            irs[token] = Union.IR(token, union_types)

        all_typedefs = self._db.find_by_kind(DatabaseBody.Kind.TYPEDEF)
        for typedef in all_typedefs.values():
            if not typedef.idl_type.is_union or typedef.idl_type.is_phantom:
                continue
            token = Union.unique_token(typedef.idl_type)
            irs[token].typedefs.append(typedef)

        for ir_i in irs.values():
            for ir_j in irs.values():
                if ir_i.contains(ir_j):
                    ir_i.sub_union_irs.append(ir_j)

        for ir in sorted(irs.values()):
            union = Union(ir)
            # Make all UnionType instances point to the same Union.
            for union_idl_type in union.idl_types:
                union_idl_type.set_union_definition_object(union)
            self._db.register(DatabaseBody.Kind.UNION, union)

    def _create_public_observable_arrays(self):
        # ObservableArrayType instances with the same element type are
        # indistinguishable (in an __eq__() and __hash__() sense).
        #
        # We go through all attributes that are ObservableArrayTypes, group the
        # indistinguishable ones together and later assign one ObservableArray
        # to all items in the group.
        @dataclasses.dataclass
        class ObservableArrayTypeInfo(object):
            attributes: typing.List[Attribute] = dataclasses.field(
                default_factory=list)
            for_testing: bool = True
            idl_types: typing.List[IdlType] = dataclasses.field(
                default_factory=list)

        grouped_type_info = collections.defaultdict(ObservableArrayTypeInfo)

        for interface in (self._db.find_by_kind(
                DatabaseBody.Kind.INTERFACE).values()):
            for attribute in interface.attributes:
                idl_type = attribute.idl_type.unwrap()
                if not idl_type.is_observable_array:
                    continue
                if not interface.code_generator_info.for_testing:
                    grouped_type_info[idl_type].for_testing = False
                grouped_type_info[idl_type].attributes.append(attribute)
                grouped_type_info[idl_type].idl_types.append(idl_type)

        for idl_type_info in grouped_type_info.values():
            # All the types in idl_types are indistinguishable; pick one for
            # ObservableArray.
            observable_array = ObservableArray(idl_type_info.idl_types[0],
                                               idl_type_info.attributes,
                                               idl_type_info.for_testing)
            for idl_type in idl_type_info.idl_types:
                if idl_type.observable_array_definition_object:
                    # When an IDL attribute is declared in an IDL interface
                    # mixin, it's possible that the exactly same
                    # web_idl.Attribute is held in two (or more)
                    # web_idl.Interfaces. Then, it's possible that
                    # set_observable_array_definition_object has already been
                    # called.
                    assert (idl_type.observable_array_definition_object is
                            observable_array)
                    continue
                idl_type.set_observable_array_definition_object(
                    observable_array)
            self._db.register(DatabaseBody.Kind.OBSERVABLE_ARRAY,
                              observable_array)

    def _assign_tags(self):

        def assign_tags_for_tree(old_ir, next_tag):
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)
            assert new_ir.tag is None
            assert new_ir.max_subclass_tag is None

            new_ir.tag = next_tag
            new_ir.max_subclass_tag = next_tag + len(new_ir.subclasses)
            next_tag += 1
            for direct_subclass in new_ir.direct_subclasses:
                next_tag = assign_tags_for_tree(direct_subclass, next_tag)
            assert next_tag == new_ir.max_subclass_tag + 1
            return next_tag

        next_tag = 256

        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.ASYNC_ITERATOR,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE,
                                            IRMap.IR.Kind.SYNC_ITERATOR)
        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            # Inheritance trees will be processed together. Always start from
            # the root.
            if old_ir.inherited is None:
                next_tag = assign_tags_for_tree(old_ir, next_tag)

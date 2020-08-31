# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import itertools
import posixpath

from blinkbuild.name_style_converter import NameStyleConverter

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
from .idl_type import IdlTypeFactory
from .interface import Interface
from .interface import LegacyWindowAlias
from .ir_map import IRMap
from .make_copy import make_copy
from .namespace import Namespace
from .operation import OperationGroup
from .reference import RefByIdFactory
from .typedef import Typedef
from .union import Union
from .user_defined_type import StubUserDefinedType
from .user_defined_type import UserDefinedType
from .validator import validate_after_resolve_references


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

        # Remove the interface members that are specific to the old bindings
        # generator, i.e. that are not necessary for (or even harmful to) the
        # new bindings generator.
        self._remove_legacy_interface_members()

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

        # Temporary mitigation of misuse of [HTMLConstructor]
        # This should be removed once the IDL definitions get fixed.
        self._supplement_missing_html_constructor_operation()

        self._copy_named_constructor_extattrs()

        # Make groups of overloaded functions including inherited ones.
        self._group_overloaded_functions()
        self._propagate_extattrs_to_overload_group()
        self._calculate_group_exposure()

        self._fill_exposed_constructs()

        self._sort_dictionary_members()

        # Updates on IRs are finished.  Create API objects.
        self._create_public_objects()

        # Resolve references.
        self._resolve_references_to_idl_def()
        self._resolve_references_to_idl_type()
        validate_after_resolve_references(self._ir_map)

        # Build union API objects.
        self._create_public_unions()

        return Database(self._db)

    def _maybe_make_copy(self, ir):
        # You can make this function return make_copy(ir) for debugging
        # purpose, etc.
        return ir  # Skip copying as an optimization.

    def _remove_legacy_interface_members(self):
        old_irs = self._ir_map.irs_of_kinds(
            IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN)

        not_disabled = (
            lambda x: 'DisableInNewIDLCompiler' not in x.extended_attributes)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = make_copy(old_ir)
            self._ir_map.add(new_ir)
            new_ir.attributes = filter(not_disabled, new_ir.attributes)
            new_ir.operations = filter(not_disabled, new_ir.operations)

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
            is_mixin = False
            if 'LegacyTreatAsPartialInterface' in new_ir.extended_attributes:
                is_partial = True
            elif hasattr(new_ir, 'is_partial') and new_ir.is_partial:
                is_partial = True
            elif hasattr(new_ir, 'is_mixin') and new_ir.is_mixin:
                is_mixin = True
            for member in new_ir.iter_all_members():
                member.code_generator_info.set_defined_in_partial(is_partial)
                member.code_generator_info.set_defined_in_mixin(is_mixin)

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
            ir = self._maybe_make_copy(ir)
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
            IRMap.IR.Kind.CALLBACK_INTERFACE, IRMap.IR.Kind.DICTIONARY,
            IRMap.IR.Kind.INTERFACE, IRMap.IR.Kind.INTERFACE_MIXIN,
            IRMap.IR.Kind.NAMESPACE, IRMap.IR.Kind.PARTIAL_DICTIONARY,
            IRMap.IR.Kind.PARTIAL_INTERFACE,
            IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN,
            IRMap.IR.Kind.PARTIAL_NAMESPACE)

        self._ir_map.move_to_new_phase()

        map(process_interface_like, old_irs)

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

            if (new_ir.is_mixin and 'LegacyTreatAsPartialInterface' not in
                    new_ir.extended_attributes):
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
                if (new_ir_headers is not None
                        and to_be_merged_headers is not None):
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

        identifier_to_derived_set = {}

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

                identifier_to_derived_set.setdefault(
                    interface.identifier, set()).add(new_interface.identifier)

        for new_interface in self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE):
            assert not new_interface.deriveds
            derived_set = identifier_to_derived_set.get(
                new_interface.identifier, set())
            new_interface.deriveds = map(
                lambda id_: self._ref_to_idl_def_factory.create(id_),
                sorted(derived_set))

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

    def _copy_named_constructor_extattrs(self):
        old_irs = self._ir_map.irs_of_kind(IRMap.IR.Kind.INTERFACE)

        self._ir_map.move_to_new_phase()

        def copy_extattrs(ext_attrs, ir):
            if 'NamedConstructor_CallWith' in ext_attrs:
                ir.extended_attributes.append(
                    ExtendedAttribute(
                        key='CallWith',
                        values=ext_attrs.values_of(
                            'NamedConstructor_CallWith')))
            if 'NamedConstructor_RaisesException' in ext_attrs:
                ir.extended_attributes.append(
                    ExtendedAttribute(key='RaisesException'))

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)
            for named_constructor_ir in new_ir.named_constructors:
                copy_extattrs(new_ir.extended_attributes, named_constructor_ir)

    def _group_overloaded_functions(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE)

        self._ir_map.move_to_new_phase()

        def make_groups(group_ir_class, operations):
            sort_key = lambda x: x.identifier
            return [
                group_ir_class(list(operations_in_group))
                for identifier, operations_in_group in itertools.groupby(
                    sorted(operations, key=sort_key), key=sort_key)
                if identifier
            ]

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            assert not new_ir.constructor_groups
            assert not new_ir.named_constructor_groups
            assert not new_ir.operation_groups
            new_ir.constructor_groups = make_groups(ConstructorGroup.IR,
                                                    new_ir.constructors)
            new_ir.named_constructor_groups = make_groups(
                ConstructorGroup.IR, new_ir.named_constructors)
            new_ir.operation_groups = make_groups(OperationGroup.IR,
                                                  new_ir.operations)

            if not isinstance(new_ir, Interface.IR):
                continue

            for item in (new_ir.iterable, new_ir.maplike, new_ir.setlike):
                if item:
                    assert not item.operation_groups
                    item.operation_groups = make_groups(
                        OperationGroup.IR, item.operations)

    def _propagate_extattrs_to_overload_group(self):
        ANY_OF = ('CrossOrigin', 'Custom', 'LegacyLenientThis',
                  'LegacyUnforgeable', 'NoAllocDirectCall', 'NotEnumerable',
                  'PerWorldBindings', 'SecureContext', 'Unscopable')

        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE)

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
                if all((overload.extended_attributes.value_of('Affects') ==
                        'Nothing') for overload in group):
                    group.extended_attributes.append(
                        ExtendedAttribute(key='Affects', values='Nothing'))

    def _calculate_group_exposure(self):
        old_irs = self._ir_map.irs_of_kinds(IRMap.IR.Kind.CALLBACK_INTERFACE,
                                            IRMap.IR.Kind.INTERFACE,
                                            IRMap.IR.Kind.NAMESPACE)

        self._ir_map.move_to_new_phase()

        for old_ir in old_irs:
            new_ir = self._maybe_make_copy(old_ir)
            self._ir_map.add(new_ir)

            for group in new_ir.iter_all_overload_groups():
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
            global_names = new_ir.extended_attributes.values_of('Global')
            if not global_names:
                continue
            constructs = set()
            for global_name in global_names:
                constructs.update(exposed_map.get(global_name, []))
            new_ir.exposed_constructs = map(
                self._ref_to_idl_def_factory.create, sorted(constructs))

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
            if idl_type.is_union:
                all_union_types.append(idl_type)

        self._idl_type_factory.for_each(collect_unions)

        def unique_key(union_type):
            """
            Returns an unique (but meaningless) key.  Returns the same key for
            the identical union types.
            """
            # TODO(peria, yukishiino): Produce unique union names.  Trying to
            # produce the names compatible to the old bindings generator for
            # the time being.
            key_pieces = []

            def flatten_member_types(idl_type):
                idl_type = idl_type.unwrap()
                if idl_type.is_union:
                    for member_type in idl_type.member_types:
                        flatten_member_types(member_type)
                else:
                    key_pieces.append(idl_type.syntactic_form)

            flatten_member_types(union_type)
            return '|'.join(key_pieces)

        grouped_unions = {}  # {unique key: list of union types}
        for union_type in all_union_types:
            key = unique_key(union_type)
            grouped_unions.setdefault(key, []).append(union_type)

        grouped_typedefs = {}  # {unique key: list of typedefs to the union}
        all_typedefs = self._db.find_by_kind(DatabaseBody.Kind.TYPEDEF)
        for typedef in all_typedefs.values():
            if not typedef.idl_type.is_union:
                continue
            key = unique_key(typedef.idl_type)
            grouped_typedefs.setdefault(key, []).append(typedef)

        for key, union_types in grouped_unions.items():
            self._db.register(
                DatabaseBody.Kind.UNION,
                Union(
                    union_types=union_types,
                    typedef_backrefs=grouped_typedefs.get(key, [])))

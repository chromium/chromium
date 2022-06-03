# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import Identifier
from .composition_parts import WithIdentifier


class IRMap(object):
    """
    Manages an identifier-IR map, where IR is IdIRMap.IR.  This class is
    designed to work together with IdlCompiler closely.  See also
    IdlCompiler, especially how IdlCompiler uses compilation phases.

    This class manages IDL definitions' IRs in following style.

        [ # phase 0
          {
            kind : { identifier : definition(s) },
            ...
          },
          # phase 1
          ...
        ]

    The outermost list represents compilation phases.  Each phase stores the
    IRs that are processed in that compilation phase.  IRs at the initial
    phase (= phase 0) is a direct translation from an AST.
    The innermost dict maps an identifier to an IR at a certain phase(*1).

    A certain phase is responsible to process certain group(s) of IR.Kind,
    so not all phases have all the kinds, there may be missing kinds in a
    certain phase.

    (*1) In case of partial definitions and includes, the dict maps
         an identifier to a list of IRs, as there may be multiple
         definitions with the same identifier.
    """

    class IR(WithIdentifier):
        """
        Represents an intermediate representation of IDL definitions used in
        IdlCompiler.

        IR is used only by IdlCompiler. See also IdlCompiler and its compilation
        strategy.  IR supports identifier-IR maps grouped by Kind (see below).
        """

        class Kind(object):
            """
            Enumerates supported kinds of IDL definitions in IR, such as
            "interface", "typedef", or "partial dictionary".
            """

            CALLBACK_FUNCTION = 'callback function'
            CALLBACK_INTERFACE = 'callback interface'
            DICTIONARY = 'dictionary'
            ENUMERATION = 'enumeration'
            INCLUDES = 'includes'
            INTERFACE = 'interface'
            INTERFACE_MIXIN = 'interface mixin'
            NAMESPACE = 'namespace'
            PARTIAL_DICTIONARY = 'partial dictionary'
            PARTIAL_INTERFACE = 'partial interface'
            PARTIAL_INTERFACE_MIXIN = 'partial interface mixin'
            PARTIAL_NAMESPACE = 'partial namespace'
            TYPEDEF = 'typedef'

            _MULTI_VALUE_KINDS = (
                INCLUDES,
                PARTIAL_DICTIONARY,
                PARTIAL_INTERFACE,
                PARTIAL_INTERFACE_MIXIN,
                PARTIAL_NAMESPACE,
            )

            @classmethod
            def does_support_multiple_defs(cls, kind):
                return kind in cls._MULTI_VALUE_KINDS

        def __init__(self, identifier, kind):
            WithIdentifier.__init__(self, identifier)
            self._kind = kind

        @property
        def kind(self):
            return self._kind

        @property
        def does_support_multiple_defs(self):
            """
            Returns True if multiple IRs may have a same identifier.

            For most kinds, an identifier points to a single IR.  However, it's
            reasonable and convenient to allow multiple IRs to have the same
            identifier for some kinds, e.g. partial interface and includes.
            This function returns True for such kinds.
            """
            return IRMap.IR.Kind.does_support_multiple_defs(self.kind)

    def __init__(self):
        # IRs whose does_support_multiple_defs is False
        self._single_value_irs = [dict()]
        # IRs whose does_support_multiple_defs is True
        self._multiple_value_irs = [dict()]

        self._current_phase = 0

    def move_to_new_phase(self):
        assert len(self._single_value_irs) == self._current_phase + 1
        assert len(self._multiple_value_irs) == self._current_phase + 1

        self._current_phase += 1
        self._single_value_irs.append(dict())
        self._multiple_value_irs.append(dict())

    def register(self, ir):
        """
        Registers the given IR to this map.

        Duplicated registration is not allowed.  The registration must be for
        the first time.
        """
        assert isinstance(ir, IRMap.IR), ir
        # Assert |ir| doesn't yet exist in this map.
        try:
            if ir.does_support_multiple_defs:
                irs = self.find_by_kind(ir.kind)
                assert ir not in irs[ir.identifier]
            else:
                duplicated_ir = self.find_by_identifier(ir.identifier)
                # We don't allow to declare a definition of an IDL definition in
                # multiple places.
                raise ValueError('{} {} is defined twice.\n  {}\n  {}'.format(
                    ir.kind, ir.identifier, ir.debug_info.location,
                    duplicated_ir.debug_info.location))
        except KeyError:
            pass
        self.add(ir)

    def add(self, ir):
        assert isinstance(ir, IRMap.IR)

        ir_map = (self._multiple_value_irs
                  if ir.does_support_multiple_defs else self._single_value_irs)
        current_irs = ir_map[self._current_phase]
        kind = ir.kind
        if kind not in current_irs:
            current_irs[kind] = dict()
        irs_per_kind = current_irs[kind]

        identifier = ir.identifier
        if ir.does_support_multiple_defs:
            if identifier not in irs_per_kind:
                irs_per_kind[identifier] = []
            irs_per_kind[identifier].append(ir)
        else:
            assert identifier not in irs_per_kind, (
                'Duplicated definition: {}\n  {}\n  {}'.format(
                    identifier, ir.debug_info.location,
                    irs_per_kind[identifier].debug_info.location))
            irs_per_kind[identifier] = ir

    def find_by_identifier(self, identifier):
        """
        Returns the latest IR whose identifier is |identifier| and
        |does_support_multiple_defs| is False.  Raises KeyError if not found.
        """
        assert isinstance(identifier, Identifier)
        for irs_per_phase in self._single_value_irs[self._current_phase::-1]:
            for irs_per_kind in irs_per_phase.values():
                if identifier in irs_per_kind:
                    return irs_per_kind[identifier]
        raise KeyError(identifier)

    def find_by_kind(self, kind):
        """
        Returns a map from identifiers to the latest IRs of |kind|.  Returns an
        empty map if not found.
        """
        ir_map = (self._multiple_value_irs
                  if IRMap.IR.Kind.does_support_multiple_defs(kind) else
                  self._single_value_irs)
        for irs_per_phase in ir_map[self._current_phase::-1]:
            if kind in irs_per_phase:
                return irs_per_phase[kind]
        return dict()

    def irs_of_kind(self, kind):
        """Returns a flattened list of IRs of the given kind."""
        if IRMap.IR.Kind.does_support_multiple_defs(kind):
            accumulated = []
            for irs in self.find_by_kind(kind).values():
                accumulated.extend(irs)
            return accumulated
        else:
            return list(self.find_by_kind(kind).values())

    def irs_of_kinds(self, *kinds):
        """
        Returns a flattened and concatenated list of IRs of all given kinds.
        """
        accumulated = []
        for kind in kinds:
            accumulated.extend(self.irs_of_kind(kind))
        return accumulated

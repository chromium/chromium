# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath

from .code_generator_info import CodeGeneratorInfo
from .code_generator_info import CodeGeneratorInfoMutable
from .exposure import Exposure
from .exposure import ExposureMutable
from .extended_attribute import ExtendedAttributes
from .extended_attribute import ExtendedAttributesMutable


class Identifier(str):
    pass


class WithIdentifier(object):
    """Implements |identifier| as a readonly attribute."""

    def __init__(self, identifier):
        if isinstance(identifier, WithIdentifier):
            identifier = identifier.identifier
        assert isinstance(identifier, Identifier)

        self._identifier = identifier

    @property
    def identifier(self):
        return self._identifier

    def change_identifier(self, new_identifier):
        assert isinstance(new_identifier, Identifier)
        self._identifier = new_identifier


class WithExtendedAttributes(object):
    """Implements |extended_attributes| as a readonly attribute."""

    def __init__(self, extended_attributes=None, readonly=False):
        if isinstance(extended_attributes, WithExtendedAttributes):
            extended_attributes = extended_attributes.extended_attributes
        elif extended_attributes is None:
            extended_attributes = ExtendedAttributes()
        assert isinstance(extended_attributes, ExtendedAttributes)

        if readonly:
            self._extended_attributes = ExtendedAttributes(extended_attributes)
        else:
            self._extended_attributes = ExtendedAttributesMutable(
                extended_attributes)

    @property
    def extended_attributes(self):
        return self._extended_attributes


class WithCodeGeneratorInfo(object):
    """Implements |code_generator_info| as a readonly attribute."""

    def __init__(self, code_generator_info=None, readonly=False):
        if isinstance(code_generator_info, WithCodeGeneratorInfo):
            code_generator_info = code_generator_info.code_generator_info
        elif code_generator_info is None:
            code_generator_info = CodeGeneratorInfoMutable()
        assert isinstance(code_generator_info, CodeGeneratorInfo)
        assert isinstance(readonly, bool)

        if readonly:
            self._code_generator_info = CodeGeneratorInfo(code_generator_info)
        else:
            self._code_generator_info = code_generator_info

    @property
    def code_generator_info(self):
        return self._code_generator_info


class WithExposure(object):
    """Implements |exposure| as a readonly attribute."""

    def __init__(self, exposure=None, readonly=False):
        if isinstance(exposure, WithExposure):
            exposure = exposure.exposure
        elif exposure is None:
            exposure = ExposureMutable()
        assert isinstance(exposure, Exposure)
        assert isinstance(readonly, bool)

        if readonly:
            self._exposure = Exposure(exposure)
        else:
            self._exposure = exposure

    @property
    def exposure(self):
        return self._exposure


class Component(str):
    """
    Represents a component that is a Blink-specific layering concept, such as
    'core' and 'modules'.
    """


class WithComponent(object):
    """
    Implements |components| as a readonly attribute.

    A single IDL definition such as 'interface' may consist from multiple IDL
    fragments like partial interfaces and mixins, which may exist across
    Blink components.  |components| is a list of Blink components of IDL
    fragments that are involved into this object.
    """

    def __init__(self, component, readonly=False):
        if isinstance(component, WithComponent):
            components = component._components
        elif isinstance(component, Component):
            components = [component]
        else:
            components = component
        assert (isinstance(components, (list, tuple)) and all(
            isinstance(component, Component) for component in components))
        assert isinstance(readonly, bool)

        if readonly:
            self._components = tuple(components)
        else:
            self._components = components

    @property
    def components(self):
        return self._components

    def add_components(self, components):
        assert isinstance(components, (list, tuple)) and all(
            isinstance(component, Component) for component in components)
        for component in components:
            if component not in self._components:
                self._components.append(component)


class Location(object):
    _blink_path_prefix = posixpath.sep + posixpath.join(
        'third_party', 'blink', 'renderer', '')

    def __init__(self, filepath=None, line_number=None, position=None):
        assert filepath is None or isinstance(filepath, str)
        assert line_number is None or isinstance(line_number, int)
        assert position is None or isinstance(position, int)

        # idl_parser produces paths based on the working directory, which may
        # not be the project root directory, e.g. "../../third_party/blink/...".
        # Canonicalize the paths heuristically.
        if filepath is not None:
            index = filepath.find(self._blink_path_prefix)
            if index >= 0:
                filepath = filepath[index + 1:]

        self._filepath = filepath
        self._line_number = line_number
        self._position = position  # Position number in a file

    def __str__(self):
        text = '{}'.format(self._filepath or '<<unknown path>>')
        if self._line_number:
            text += ':{}'.format(self._line_number)
        return text

    @property
    def filepath(self):
        return self._filepath

    @property
    def line_number(self):
        return self._line_number

    @property
    def position_in_file(self):
        return self._position


class DebugInfo(object):
    """Provides information useful for debugging."""

    def __init__(self, location=None):
        assert location is None or isinstance(location, Location)
        # The first entry is the primary location, e.g. location of non-partial
        # interface.  The rest is secondary locations, e.g. location of partial
        # interfaces and mixins.
        self._locations = [location] if location else []

    @property
    def location(self):
        """
        Returns the primary location, i.e. location of the main definition.
        """
        return self._locations[0] if self._locations else Location()

    @property
    def all_locations(self):
        """
        Returns a list of locations of all related IDL definitions, including
        partial definitions and mixins.
        """
        return tuple(self._locations)

    def add_locations(self, locations):
        assert isinstance(locations, (list, tuple)) and all(
            isinstance(location, Location) for location in locations)
        self._locations.extend(locations)


class WithDebugInfo(object):
    """Implements |debug_info| as a readonly attribute."""

    def __init__(self, debug_info=None):
        if isinstance(debug_info, WithDebugInfo):
            debug_info = debug_info.debug_info
        elif debug_info is None:
            debug_info = DebugInfo()
        assert isinstance(debug_info, DebugInfo)

        self._debug_info = debug_info

    @property
    def debug_info(self):
        return self._debug_info


class WithOwner(object):
    """Implements |owner| as a readonly attribute."""

    def __init__(self, owner):
        assert isinstance(owner, object) and owner is not None
        self._owner = owner

    @property
    def owner(self):
        return self._owner


class WithOwnerMixin(object):
    """Implements |owner_mixin| as a readonly attribute."""

    def __init__(self, owner_mixin=None):
        if isinstance(owner_mixin, WithOwnerMixin):
            owner_mixin = owner_mixin._owner_mixin
        # In Python2, we need to avoid circular imports.
        from .reference import RefById
        assert owner_mixin is None or isinstance(owner_mixin, RefById)

        self._owner_mixin = owner_mixin

    @property
    def owner_mixin(self):
        """
        Returns the interface mixin object where this construct was originally
        defined.
        """
        return self._owner_mixin.target_object if self._owner_mixin else None

    def set_owner_mixin(self, mixin):
        # In Python2, we need to avoid circular imports.
        from .reference import RefById
        assert isinstance(mixin, RefById)
        assert self._owner_mixin is None
        self._owner_mixin = mixin

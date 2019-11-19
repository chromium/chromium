# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .code_generator_info import CodeGeneratorInfo
from .code_generator_info import CodeGeneratorInfoMutable
from .exposure import Exposure
from .exposure import ExposureMutable
from .extended_attribute import ExtendedAttributes


class Identifier(str):
    pass


class WithIdentifier(object):
    """Implements |identifier| as a readonly attribute."""

    def __init__(self, identifier):
        assert isinstance(identifier, Identifier)
        self._identifier = identifier

    @property
    def identifier(self):
        return self._identifier


class WithExtendedAttributes(object):
    """Implements |extended_attributes| as a readonly attribute."""

    def __init__(self, extended_attributes=None):
        assert (extended_attributes is None
                or isinstance(extended_attributes, ExtendedAttributes))
        self._extended_attributes = extended_attributes or ExtendedAttributes()

    @property
    def extended_attributes(self):
        return self._extended_attributes


class WithCodeGeneratorInfo(object):
    """Implements |code_generator_info| as a readonly attribute."""

    def __init__(self, code_generator_info=None):
        assert (code_generator_info is None
                or isinstance(code_generator_info, CodeGeneratorInfo))
        self._code_generator_info = (code_generator_info
                                     or CodeGeneratorInfoMutable())

    @property
    def code_generator_info(self):
        return self._code_generator_info


class WithExposure(object):
    """Implements |exposure| as a readonly attribute."""

    def __init__(self, exposure=None):
        assert exposure is None or isinstance(exposure, Exposure)
        self._exposure = exposure or ExposureMutable()

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

    def __init__(self, component=None, components=None):
        """
        Args:
            component:
            components: Either of |component| or |components| must be given.
        """
        assert component is None or isinstance(component, Component)
        assert components is None or (isinstance(components, (list, tuple))
                                      and all(
                                          isinstance(component, Component)
                                          for component in components))
        assert int(component is not None) + int(components is not None) == 1
        if components is not None:
            self._components = list(components)
        elif component is not None:
            self._components = [component]

    @property
    def components(self):
        """
        Returns a list of components' names where this definition is defined
        """
        return tuple(self._components)

    def add_components(self, components):
        assert isinstance(components, (list, tuple)) and all(
            isinstance(component, Component) for component in components)
        for component in components:
            if component not in self.components:
                self._components.append(component)


class Location(object):
    def __init__(self, filepath=None, line_number=None, position=None):
        assert filepath is None or isinstance(filepath, str)
        assert line_number is None or isinstance(line_number, int)
        assert position is None or isinstance(position, int)
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

    def __init__(self, location=None, locations=None):
        assert location is None or isinstance(location, Location)
        assert locations is None or (isinstance(locations, (list, tuple))
                                     and all(
                                         isinstance(location, Location)
                                         for location in locations))
        assert not (location and locations)
        # The first entry is the primary location, e.g. location of non-partial
        # interface.  The rest is secondary locations, e.g. location of partial
        # interfaces and mixins.
        if locations:
            self._locations = locations
        else:
            self._locations = [location or Location()]

    @property
    def location(self):
        """
        Returns the primary location, i.e. location of the main definition.
        """
        return self._locations[0]

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
        assert debug_info is None or isinstance(debug_info, DebugInfo)
        self._debug_info = debug_info or DebugInfo()

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

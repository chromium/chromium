# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .attribute import Attribute
from .code_generator_info import CodeGeneratorInfoMutable
from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithIdentifier
from .idl_type import IdlType


class ObservableArray(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
                      WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-observable-array"""

    def __init__(self, idl_type, attributes, for_testing):
        assert isinstance(idl_type, IdlType)
        assert isinstance(attributes, (list, tuple)) and all(
            isinstance(attribute, Attribute) for attribute in attributes)
        assert idl_type.is_observable_array

        identifier = Identifier('ObservableArray_{}'.format(
            (idl_type.element_type.type_name_with_extended_attribute_key_values
             )))

        components = []
        element_type = idl_type.element_type.unwrap()
        component_object = (element_type.type_definition_object
                            or element_type.union_definition_object)
        if component_object:
            components.append(component_object.components[0])

        code_generator_info = CodeGeneratorInfoMutable()
        code_generator_info.set_for_testing(for_testing)

        WithIdentifier.__init__(self, identifier)
        WithCodeGeneratorInfo.__init__(self,
                                       code_generator_info,
                                       readonly=True)
        WithComponent.__init__(self, components, readonly=True)
        WithDebugInfo.__init__(self, idl_type.debug_info)

        self._idl_type = idl_type
        self._user_attributes = tuple(attributes)

    @property
    def idl_type(self):
        """Returns the type of the observable array."""
        return self._idl_type

    @property
    def element_type(self):
        """Returns the element type of the observable array."""
        return self.idl_type.element_type

    @property
    def user_attributes(self):
        """Returns the attributes that use this observable array type."""
        return self._user_attributes

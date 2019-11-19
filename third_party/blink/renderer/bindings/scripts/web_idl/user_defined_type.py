# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithComponent
from .composition_parts import WithIdentifier


class UserDefinedType(WithIdentifier):
    """
    UserDefinedType is a common base class of spec-author-defined types.

    Spec-author-defined types are top-level IDL definitions given an identifier.
    """

    def __init__(self, identifier):
        WithIdentifier.__init__(self, identifier)

    @property
    def is_interface(self):
        """Returns True if this is an IDL interface."""
        return False

    @property
    def is_dictionary(self):
        """Returns True if this is an IDL dictionary."""
        return False

    @property
    def is_callback_function(self):
        """Returns True if this is an IDL callback function."""
        return False

    @property
    def is_callback_interface(self):
        """Returns True if this is an IDL callback interface."""
        return False

    @property
    def is_enumeration(self):
        """Returns True if this is an IDL enumeration."""
        return False


class StubUserDefinedType(UserDefinedType, WithComponent):
    def __init__(self, identifier):
        UserDefinedType.__init__(self, identifier)
        WithComponent.__init__(self, components=[])

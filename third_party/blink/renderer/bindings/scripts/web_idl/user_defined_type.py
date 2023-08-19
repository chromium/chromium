# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from .composition_parts import WithComponent
from .composition_parts import WithIdentifier


class UserDefinedType(WithIdentifier):
    """
    UserDefinedType is a common base class of spec-author-defined types.

    Spec-author-defined types are top-level IDL definitions given an identifier.

    Although async/sync iterators are not top-level IDL definitions nor have an
    identifier, AsyncIterator and SyncIterator inherit from UserDefinedType
    just in order to make bind_gen.interface.generate_class_like work nicely
    with using is_interface, is_namespace, etc.
    """

    def __init__(self, identifier):
        WithIdentifier.__init__(self, identifier)

    @property
    def is_callback_function(self):
        """Returns True if this is an IDL callback function."""
        return False

    @property
    def is_callback_interface(self):
        """Returns True if this is an IDL callback interface."""
        return False

    @property
    def is_dictionary(self):
        """Returns True if this is an IDL dictionary."""
        return False

    @property
    def is_enumeration(self):
        """Returns True if this is an IDL enumeration."""
        return False

    @property
    def is_interface(self):
        """Returns True if this is an IDL interface."""
        return False

    @property
    def is_namespace(self):
        """Returns True if this is an IDL namespace."""
        return False

    @property
    def is_async_iterator(self):
        """Returns True if this is a async iterator."""
        return False

    @property
    def is_sync_iterator(self):
        """Returns True if this is a sync iterator."""
        return False


class StubUserDefinedType(UserDefinedType, WithComponent):
    def __init__(self, identifier):
        UserDefinedType.__init__(self, identifier)
        WithComponent.__init__(self, [])

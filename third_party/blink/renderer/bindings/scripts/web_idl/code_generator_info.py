# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The list of attributes that CodeGeneratorInfo supports.  CodeGeneratorInfo's
# attributes are auto-generated from this list because they're boilerplated.
_CODE_GENERATOR_INFO_ATTRIBUTES = (
    'blink_headers',
    'defined_in_partial',
    'for_testing',
    'is_active_script_wrappable',
    'is_legacy_unenumerable_named_properties',
    'property_implemented_as',
    'receiver_implemented_as',
)

_CGI_ATTRS = tuple([
    # attribute name (_foo of self._foo), getter name, setter name
    ('_{}'.format(attr), '{}'.format(attr), 'set_{}'.format(attr))
    for attr in _CODE_GENERATOR_INFO_ATTRIBUTES
])


class CodeGeneratorInfo(object):
    """A bag of properties to be used by bindings code generators."""

    def __init__(self, other=None):
        """
        Initializes a new object.

        If |other| is not None, initializes the new object as a copy of |other|.
        If |other| is None, initializes the new object as empty.
        """
        assert other is None or isinstance(other, CodeGeneratorInfo)

        if other is None:
            for attr_name, _, _ in _CGI_ATTRS:
                setattr(self, attr_name, None)
        else:
            for attr_name, _, _ in _CGI_ATTRS:
                setattr(self, attr_name, getattr(other, attr_name))

    @staticmethod
    def make_getter(attr_name):
        @property
        def getter(self):
            return getattr(self, attr_name)

        return getter


class CodeGeneratorInfoMutable(CodeGeneratorInfo):
    """Another version of CodeGeneratorInfo that supports setters."""

    @staticmethod
    def make_setter(attr_name):
        def setter(self, value):
            setattr(self, attr_name, value)

        return setter

    def __getstate__(self):
        assert False, "CodeGeneratorInfoMutable must not be pickled."

    def __setstate__(self, state):
        assert False, "CodeGeneratorInfoMutable must not be pickled."


for attr_name, getter_name, setter_name in _CGI_ATTRS:
    setattr(CodeGeneratorInfo, getter_name,
            CodeGeneratorInfo.make_getter(attr_name))
    setattr(CodeGeneratorInfoMutable, setter_name,
            CodeGeneratorInfoMutable.make_setter(attr_name))

CodeGeneratorInfo.make_getter = None
CodeGeneratorInfoMutable.make_setter = None

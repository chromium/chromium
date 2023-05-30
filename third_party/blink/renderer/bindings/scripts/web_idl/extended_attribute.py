# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools


class ExtendedAttribute(object):
    """
    Represents a single extended attribute.
    https://webidl.spec.whatwg.org/#dfn-extended-attribute
    """

    # [Key]
    _FORM_NO_ARGS = 'NoArgs'
    # [Key=Value]
    _FORM_IDENT = 'Ident'
    # [Key=(Value1, Value2, ...)]
    _FORM_IDENT_LIST = 'IdentList'
    # [Key(Value1L Value1R, Value2L Value2R, ...)]
    _FORM_ARG_LIST = 'ArgList'
    # [Key=Name(Value1L Value1R, Value2L Value2R, ...)]
    _FORM_NAMED_ARG_LIST = 'NamedArgList'

    def __init__(self, key, values=None, arguments=None, name=None):
        assert isinstance(key, str)
        assert values is None or isinstance(values, str) or (isinstance(
            values,
            (list, tuple)) and all(isinstance(value, str) for value in values))
        assert arguments is None or (isinstance(
            arguments, (list, tuple)) and all(
                isinstance(left, str) and isinstance(right, str)
                for left, right in arguments))
        assert name is None or isinstance(name, str)

        self._format = None
        self._key = key
        self._values = None
        self._arguments = None
        self._name = name

        if name is not None:
            self._format = self._FORM_NAMED_ARG_LIST
            if values is not None or arguments is None:
                raise ValueError('Unknown format for ExtendedAttribute')
            self._arguments = tuple(arguments)
        elif arguments is not None:
            self._format = self._FORM_ARG_LIST
            if values is not None:
                raise ValueError('Unknown format for ExtendedAttribute')
            self._arguments = tuple(arguments)
        elif values is None:
            self._format = self._FORM_NO_ARGS
        elif isinstance(values, str):
            self._format = self._FORM_IDENT
            self._values = values
        else:
            self._format = self._FORM_IDENT_LIST
            self._values = tuple(values)

    @classmethod
    def equals(cls, lhs, rhs):
        """
        Returns True if |lhs| and |rhs| have the same contents.

        Note that |lhs == rhs| evaluates to True if lhs and rhs are the same
        object.
        """
        if lhs is None and rhs is None:
            return True
        if not all(isinstance(x, cls) for x in (lhs, rhs)):
            return False

        return (lhs.key == rhs.key
                and lhs.syntactic_form == rhs.syntactic_form)

    @property
    def syntactic_form(self):
        if self._format == self._FORM_NO_ARGS:
            return self._key
        if self._format == self._FORM_IDENT:
            return '{}={}'.format(self._key, self._values)
        if self._format == self._FORM_IDENT_LIST:
            return '{}=({})'.format(self._key, ', '.join(self._values))
        args_str = '({})'.format(', '.join(
            ['{} {}'.format(left, right) for left, right in self._arguments]))
        if self._format == self._FORM_ARG_LIST:
            return '{}{}'.format(self._key, args_str)
        if self._format == self._FORM_NAMED_ARG_LIST:
            return '{}={}{}'.format(self._key, self._name, args_str)
        assert False, 'Unknown format: {}'.format(self._format)

    @property
    def key(self):
        return self._key

    @property
    def value(self):
        """
        Returns the value for format Ident.  Returns None for format NoArgs.
        Otherwise, raises a ValueError.
        """
        if self._format in (self._FORM_NO_ARGS, self._FORM_IDENT):
            return self._values
        raise ValueError('[{}] does not have a single value.'.format(
            self.syntactic_form))

    @property
    def has_values(self):
        return self._format in (self._FORM_NO_ARGS, self._FORM_IDENT,
                                self._FORM_IDENT_LIST)

    @property
    def values(self):
        """
        Returns a list of values for format Ident and IdentList.  Returns an
        empty list for format NorArgs.  Otherwise, raises a ValueError.
        """
        if self._format == self._FORM_NO_ARGS:
            return ()
        if self._format == self._FORM_IDENT:
            return (self._values, )
        if self._format == self._FORM_IDENT_LIST:
            return self._values
        raise ValueError('[{}] does not have a value.'.format(
            self.syntactic_form))

    @property
    def has_arguments(self):
        return self._format in (self._FORM_ARG_LIST, self._FORM_NAMED_ARG_LIST)

    @property
    def arguments(self):
        """
        Returns a list of value pairs for format ArgList and NamedArgList.
        Otherwise, raises a ValueError.
        """
        if self._format in (self._FORM_ARG_LIST, self._FORM_NAMED_ARG_LIST):
            return self._arguments
        raise ValueError('[{}] does not have an argument.'.format(
            self.syntactic_form))

    @property
    def has_name(self):
        return self._format == self._FORM_NAMED_ARG_LIST

    @property
    def name(self):
        """
        Returns |Name| for format NamedArgList.  Otherwise, raises a ValueError.
        """
        if self._format == self._FORM_NAMED_ARG_LIST:
            return self._name
        raise ValueError('[{}] does not have a name.'.format(
            self.syntactic_form))


class ExtendedAttributes(object):
    """
    ExtendedAttributes is a dict-like container for ExtendedAttribute instances.
    With a key string, you can get an ExtendedAttribute or a list of them.

    For an IDL fragment
      [A, A=(foo, bar), B=baz]
    an ExtendedAttributes instance will be like
      {
        'A': (ExtendedAttribute('A'),
              ExtendedAttribute('A', values=('foo', 'bar'))),
        'B': (ExtendedAttribute('B', value='baz')),
      }

    https://webidl.spec.whatwg.org/#idl-extended-attributes
    """

    def __init__(self, extended_attributes=None):
        assert (extended_attributes is None
                or isinstance(extended_attributes, ExtendedAttributes)
                or (isinstance(extended_attributes, (list, tuple)) and all(
                    isinstance(attr, ExtendedAttribute)
                    for attr in extended_attributes)))

        sorted_ext_attrs = sorted(
            extended_attributes or [], key=lambda x: x.key)

        self._ext_attrs = {
            key: tuple(sorted(ext_attrs, key=lambda x: x.syntactic_form))
            for key, ext_attrs in itertools.groupby(
                sorted_ext_attrs, key=lambda x: x.key)
        }
        self._keys = None
        self._length = None
        self._on_ext_attrs_updated()

    def _on_ext_attrs_updated(self):
        self._keys = tuple(sorted(self._ext_attrs.keys()))
        self._length = 0
        for ext_attrs in self._ext_attrs.values():
            self._length += len(ext_attrs)

    @classmethod
    def equals(cls, lhs, rhs):
        """
        Returns True if |lhs| and |rhs| have the same contents.

        Note that |lhs == rhs| evaluates to True if lhs and rhs are the same
        object.
        """
        if lhs is None and rhs is None:
            return True
        if not all(isinstance(x, cls) for x in (lhs, rhs)):
            return False

        if lhs.keys() != rhs.keys():
            return False
        if len(lhs) != len(rhs):
            return False
        for l, r in zip(lhs, rhs):
            if not ExtendedAttribute.equals(l, r):
                return False
        return True

    def __contains__(self, key):
        """Returns True if this has an extended attribute with the |key|."""
        return key in self._ext_attrs

    def __iter__(self):
        """Yields all ExtendedAttribute instances in a certain sorted order."""
        for key in self._keys:
            for ext_attr in self._ext_attrs[key]:
                yield ext_attr

    def __len__(self):
        return self._length

    @property
    def syntactic_form(self):
        return '[{}]'.format(', '.join(
            [ext_attr.syntactic_form for ext_attr in self]))

    def keys(self):
        return self._keys

    def get(self, key):
        """
        Returns an exnteded attribute whose key is |key|, or None if not found.
        If there are multiple extended attributes with |key|, raises an error.
        """
        values = self.get_list_of(key)
        if len(values) == 0:
            return None
        if len(values) == 1:
            return values[0]
        raise ValueError(
            "There are multiple extended attributes for the key '{}'.".format(
                key))

    def get_list_of(self, key):
        """
        Returns a list of extended attributes whose keys are |key|.
        """
        return self._ext_attrs.get(key, ())

    def value_of(self, key):
        """Returns self.get(key).value if the key exists or None."""
        ext_attr = self.get(key)
        return ext_attr.value if ext_attr else None

    def values_of(self, key):
        """Returns self.get(key).values if the key exists or an empty list."""
        ext_attr = self.get(key)
        return ext_attr.values if ext_attr else ()

    def _append(self, ext_attr):
        assert isinstance(ext_attr, ExtendedAttribute)

        if ext_attr.key not in self._ext_attrs:
            self._ext_attrs[ext_attr.key] = (ext_attr, )
        else:
            self._ext_attrs[ext_attr.key] = (tuple(
                sorted(
                    self._ext_attrs[ext_attr.key] + (ext_attr, ),
                    key=lambda x: x.syntactic_form)))
        self._on_ext_attrs_updated()


class ExtendedAttributesMutable(ExtendedAttributes):
    def __getstate__(self):
        assert False, "ExtendedAttributesMutable must not be pickled."

    def __setstate__(self, state):
        assert False, "ExtendedAttributesMutable must not be pickled."

    def append(self, ext_attr):
        self._append(ext_attr)

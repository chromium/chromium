# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utilities for creating starlark values.

The convert_direct function converts python values when the starlark
value is the direct translation. The convert_swarming function converts
the swarming dict present in pyl files to a targets.swarming call.

3 classes are provided that enable building compound values for starlark
output: CallValueBuilder, DictValueBuilder and ListValueBuilder for
building function call, dict literal and list literal values,
respectively. Objects of each class can be initialized with elements
and/or updated using the [] operator for CallValueBuilder and
DictValueBuilder or the append method for ListValueBuilder.

The text form of the value can be obtained from all 3 classes by calling
the output method.

The parameter values of a call, item values of a dict or elements of a
list can be either strings or value builders. Value builders can contain
0 or more entries (arguments in a function call, dict items, list
elements). Unless the value builder is constructed with
output_empty=True, if the value builder has no entries, it will not
produce any output. In that case, any entry using that value will be
ignored when producing output for the containing object.

This makes it simple to combine ValueBuilders in specified orders
without having to worry about conditonalizing their creation while
matching on dict keys in a loop.

e.g.
  must_be_first_if_present_builder = CallBuilder('foo')
  list_builder = ListBuilder([must_be_first_if_present_builder])

  for k, v in d.items():
    match k:
      case 'foo':
        must_be_first_if_present_builder['arg'] = value

      case _:
        list_builder.append(v)

Given
  d = {'bar': 'x', 'baz': 'y', 'foo': 'z'}
list_builder.output will be
  [
    foo(
      arg = z,
    ),
    'x',
    'y',
  ]

Given
  d = {'bar', 'x', 'baz', 'y'}
list_builder.output will be
  [
    'x',
    'y',
  ]
"""

from __future__ import annotations

import abc
import collections.abc
import typing



class ValueBuilder(abc.ABC):
  """An object that potentially builds a value for starlark output.

  A ValueBuilder can contain 0 or more entries (arguments in a function
  call, dictionary items, list elements). If the ValueBuilder contains
  no entries, it will not produce output and it will not appear in the
  output of a ValueBuilder that contains it, unless the ValueBuilder was
  created with output_empty=True.
  """

  _INDENT = '  '

  @typing.final
  def output(self) -> str | None:
    """Get the final output of the value.

    Returns:
      A string if the there is output associated with the value,
      otherwise None.
    """
    output_stream = self._output_stream('')
    if output_stream is None:
      return None
    return ''.join(output_stream)

  @abc.abstractmethod
  def _output_stream(self, indent: str) -> typing.Iterable[str] | None:
    raise NotImplementedError()  # pragma: no cover


# A value that can be contained by another value. A string value will be output
# as-is. A ValueBuilder may or may not produce output.
Value: typing.TypeAlias = str | ValueBuilder


def to_output(value: Value | CommentedValue) -> str | None:
  """Get the output for a starlark value."""
  if isinstance(value, (ValueBuilder, CommentedValue)):
    return value.output()
  return value


T = typing.TypeVar('T', bound=Value)


class CommentedValue(typing.Generic[T]):
  """A class for adding comments to a value."""

  def __init__(self, value: T, comments: list[str]):
    self._value = value
    self._comments = comments

  def output(self) -> str | None:
    """Get the final output of the value.

    Returns:
      A string if the there is output associated with the value,
      otherwise None.
    """
    output_stream = self._output_stream('')
    if output_stream is None:
      return None
    return ''.join(output_stream)

  def _output_stream(self, indent: str) -> typing.Iterable[str] | None:
    if isinstance(self._value, ValueBuilder):
      value_output = self._value._output_stream(indent)
      if value_output is None:
        return None
    else:
      value_output = [self._value]
    output_stream = []
    for comment in self._comments:
      output_stream.extend([comment, '\n', indent])
    output_stream.extend(value_output)
    return output_stream


MaybeCommentedValue: typing.TypeAlias = T | CommentedValue[T]


class _CompoundValueBuilder(ValueBuilder):
  """A class for building compound values.

  A _CompoundValueBuilder can contain 0 or more entries which themselves
  can contain ValueBuilder instances. If a contained ValueBuilder
  doesn't produce output, then the containing entry will be ignored. If
  none of the contained ValueBuilder instances produce output, then the
  _CompoundValueBuilder will also not produce any output, unless it was
  created with output_empty=True.
  """

  def __init__(self, *, output_empty: bool = False):
    """Initialize the _CompoundValueBuilder.

    Args:
      output_empty: Output the value even if it contains no entries.
    """
    self._output_empty = output_empty

  @property
  @abc.abstractmethod
  def _prefix(self) -> str:
    """The text of the opening of the compound value."""
    raise NotImplementedError()  # pragma: no cover

  @property
  @abc.abstractmethod
  def _suffix(self) -> str:
    """The text of the closing of the compound value."""
    raise NotImplementedError()  # pragma: no cover

  @abc.abstractmethod
  def _entries(self, indent: str) -> typing.Iterable[str] | None:
    """The text of the compound value's entries.

    Args:
      indent: The level of indentation that the entries are at.

    Returns:
      An iterable of strings containing the text of the entries if there
      are entries that produce text, otherwise None. The elements of the
      iterable will be concatenated in the final string.
    """
    raise NotImplementedError()  # pragma: no cover

  def _output_stream(self, indent: str) -> typing.Iterable[str] | None:
    """The text of the compound value.

    Args:
      indent: The level of indentation that the value is at.

    Returns:
      An iterable of strings containing the text of the value if there
      are entries that produce text, otherwise None. The elements of the
      iterable will be concatenated in the final string.
    """
    entries = self._entries(indent + self._INDENT)
    # We only have to indent before the suffix:
    # * The indentation preceding the value is handled by any containing value,
    #   this might not be the first thing on the line
    # * Indentation for each entry is handled by the _entries method
    # We only have to add a newline after the prefix:
    # * Newlines for each entry are handled by the _entries method
    # * The containing value may or may not need to add a command and newline
    #   after the value, so leave it be
    if entries is not None:
      return [self._prefix, '\n', *entries, indent, self._suffix]
    if self._output_empty:
      return [self._prefix, self._suffix]
    return None

  @staticmethod
  def _get_output_stream_for_contained_value(
      contained_value: Value | CommentedValue,
      indent: str,
  ) -> typing.Iterable[str] | None:
    """Get the text of a value.

    Args:
      value: The value to get the output for.
      indent: The level of indentation that the value is at.

    Returns:
      An iterable of strings containing the text of the value if there
      are entries that produce text, otherwise None. The elements of the
      iterable will be concatenated in the final string.
    """
    if isinstance(contained_value, (ValueBuilder, CommentedValue)):
      return contained_value._output_stream(indent)
    return [contained_value]


class CallValueBuilder(_CompoundValueBuilder):
  """A builder of function call expression values.

  Only function calls using keyword arguments are supported.

  The entries of a function call are the parameters to the function. The
  parameters will appear in the output in the order they were added.
  """

  ParamName: typing.TypeAlias = MaybeCommentedValue[str]

  def __init__(self,
               function: str,
               params: collections.abc.Mapping[ParamName, Value] | None = None,
               *,
               elide_param: str | None = None,
               **kwargs):
    """Initialize the CallValueBuilder.

    Args:
      function: The expression giving the function to call.
      params: Initial parameters to include. The parameters will appear
        in the output in the same order they are present in the dict and
        before any parameters added after initialization.
    """
    super().__init__(**kwargs)
    self._function: str = function
    self._params = dict(params or {})
    self._elide_param = elide_param

  def __setitem__(self, param_name: ParamName, param_value: Value) -> None:
    """Add an additional parameter to the call."""
    self._params[param_name] = param_value

  @property
  def _prefix(self) -> str:
    return f'{self._function}('

  @property
  def _suffix(self) -> str:
    return ')'

  def _output_stream(self, indent: str) -> typing.Iterable[str] | None:
    param_output_streams = {}
    for param_name, param_value in self._params.items():
      output_stream = self._get_output_stream_for_contained_value(
          param_value, indent)
      if output_stream is not None:
        param_output_streams[param_name] = output_stream

    if (self._elide_param is not None
        and self._elide_param in param_output_streams
        and len(param_output_streams) == 1):
      return param_output_streams[self._elide_param]

    return super()._output_stream(indent)

  def _entries(self, indent: str) -> typing.Iterable[str] | None:
    param_output_streams = {}
    for param_name, param_value in self._params.items():
      output_stream = self._get_output_stream_for_contained_value(
          param_value, indent)
      if output_stream is not None:
        param_output_streams[param_name] = output_stream

    if not param_output_streams:
      return None

    def gen():
      for key, output_stream in param_output_streams.items():
        yield indent
        key_stream = self._get_output_stream_for_contained_value(key, indent)
        assert key_stream
        yield from key_stream
        yield ' = '
        yield from output_stream
        yield ',\n'

    return gen()


class DictValueBuilder(_CompoundValueBuilder):
  """A builder of dict literal values.

  The entries of a dict are the items in the dict. The items will appear
  in the order they were added.
  """

  Key: typing.TypeAlias = MaybeCommentedValue[str]

  def __init__(self,
               items: collections.abc.Mapping[Key, Value] | None = None,
               **kwargs):
    """Initialize the CallValueBuilder.

    Args:
      items: Initial items to include. The items will appear in the
        output in the same order they are present in the dict and before
        any items added after initialization.
    """
    super().__init__(**kwargs)
    self._items = dict(items or {})

  def __setitem__(self, key: Key, value: Value) -> None:
    """Add an additional item to the dict."""
    self._items[key] = value

  @property
  def _prefix(self) -> str:
    return '{'

  @property
  def _suffix(self) -> str:
    return '}'

  def _entries(self, indent: str) -> typing.Iterable[str] | None:
    item_output_streams = {}
    for key, value in self._items.items():
      output_stream = self._get_output_stream_for_contained_value(value, indent)
      if output_stream is not None:
        item_output_streams[key] = output_stream

    if not item_output_streams:
      return None

    def gen():
      for key, output_stream in item_output_streams.items():
        yield indent
        key_stream = self._get_output_stream_for_contained_value(key, indent)
        assert key_stream
        yield from key_stream
        yield ': '
        yield from output_stream
        yield ',\n'

    return gen()


class ListValueBuilder(_CompoundValueBuilder):
  """A builder of list literal values.

  The entries of a list are the elements in the dict. The elements will
  appear in the order they were added.
  """

  Element: typing.TypeAlias = MaybeCommentedValue[Value]

  def __init__(
      self,
      elements: collections.abc.Iterable[MaybeCommentedValue[T]] | None = None,
      **kwargs,
  ):
    """Initialize the ListValueBuilder.

    Args:
      elements: Initial elements to include. The elements will appear in
        the output in the same order they are present in the dict and
        before any elements added after initialization.
    """
    super().__init__(**kwargs)
    self._elements: list[ListValueBuilder.Element] = list(elements or [])

  def append(self, value: Element) -> None:
    """Add an additional element to the list."""
    self._elements.append(value)

  @property
  def _prefix(self) -> str:
    return '['

  @property
  def _suffix(self) -> str:
    return ']'

  def _entries(self, indent: str = '') -> typing.Iterable[str] | None:
    element_output_streams = []
    for element in self._elements:
      output_stream = self._get_output_stream_for_contained_value(
          element, indent)
      if output_stream is not None:
        element_output_streams.append(output_stream)

    if not element_output_streams:
      return None

    def gen():
      for output_element in element_output_streams:
        yield indent
        yield from output_element
        yield ',\n'

    return gen()

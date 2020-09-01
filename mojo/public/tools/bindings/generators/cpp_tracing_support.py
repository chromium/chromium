# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ source code to trace mojo method parameters."""

from generators.cpp_util import IsNativeOnlyKind
import mojom.generate.module as mojom
from abc import ABCMeta
from abc import abstractmethod


class _OutputContext(object):
  __metaclass__ = ABCMeta
  """Represents the context in which |self.value| should be used.

  This is a base class for _ArrayItem, _DictionaryItemWithLiteralKey, and
  _DictionaryItemWithCopiedKey. The distinction between _ArrayItem and
  _DictionaryItemWithLiteralKey/_DictionaryItemWithCopiedKey is that
  _ArrayItem has no name. The distinction between
  _DictionaryItemWithLiteralKey and _DictionaryItemWithCopiedKey is whether
  the name is expected to be long lived or can be temporary.
  """

  def __init__(self, value):
    self.value = value

  @abstractmethod
  def AddSingleValue(self, trace_type, parameter_value):
    pass

  @abstractmethod
  def BeginContainer(self, container_type):
    pass

  def EndContainer(self, container_type):
    """Return a line of C++ code to close a container on self.value.

    Args:
      container_type: {string} Either 'Dictionary' or 'Array'.

    Returns:
      A single line of C++ which closes a container on self.value.
    """
    return '%s->End%s();' % (self.value, container_type)

  def AsValueInto(self, cpp_expression):
    """Return a line of C++ code to trace a variable to self.value.
    Most probably the user of this method wants to yield
    |self.BeginContainer('Dictionary')| immediately before and
    |self.EndContainer('Dictionary')| immediately after a call to
    |AsValueInto| (see documentation of |cpp_expression::AsValueInto|).

    Args:
      cpp_expression: {string} The C++ expression on which |->AsValueInto|
        will be called.

    Returns:
      A single line of C++ which calls |AsValueInto| on |cpp_expression|
      with self.value as the parameter.
    """
    return '%s->AsValueInto(%s);' % (cpp_expression, self.value)

  def TraceContainer(self, container_type, iterator_name, container_name,
                     loop_body):
    """Generate the C++ for-loop to trace the container |container_name|.

    Args:
      container_type: {string} Either 'Array' or 'Dictionary'.
      iterator_name: {string} The iterator variable name to be used.
      container_name: {string} The name of the variable holding the container.
      loop_body: {iterable} Lines of C++ code that trace individual elements.

    Yields:
      Lines of C++ for-loop to trace the container.
    """
    yield self.BeginContainer(container_type)
    yield 'for (const auto& %s : %s) {' % (iterator_name, container_name)
    for line in loop_body:
      yield '  ' + line
    yield '}'
    yield self.EndContainer(container_type)


class _ArrayItem(_OutputContext):
  """Represents a |TracedValue| which expects to receive an array item (of no
  name). This means that |AddSingleValue| will return a call on self.value
  which is an Append and |BeginContainer| will start a container with no
  name.

  Attributes:
    value: {string} The name of the C++ variable of type
      |base::trace_event::TracedValue*| this object represents.
  """

  def __init__(self, value):
    super(_ArrayItem, self).__init__(value)

  def AddSingleValue(self, trace_type, parameter_value):
    """Return a line of C++ code that will append a single value to
    |self.value|.

    Args:
      trace_type: {string} The type of the appended value. Can be one of:
        'Integer', 'Double', 'Boolean', 'String'.
      parameter_value: {string} The C++ expression to be passed as the
        appended value.

    Returns:
      A single line of C++ which appends |parameter_value| to self.value.
    """
    return '%s->Append%s(%s);' % (self.value, trace_type, parameter_value)

  def BeginContainer(self, container_type):
    """Return a line of C++ code to open a container on self.value.

    Args:
      container_type: {string} Either 'Dictionary' or 'Array'.

    Returns:
      A single line of C++ which starts a container on self.value.
    """
    return '%s->Begin%s();' % (self.value, container_type)


class _DictionaryItemWithLiteralKey(_OutputContext):
  """Represents a |TracedValue| which expects to receive a dictionary item
  (with a long lived name). This means that |AddSingleValue| will return a
  call on self.value which is a Set and |BeginContainer| will start a
  container with self.name used in a string literal.

  |base::trace_event::TracedValue| has two sets of methods -- ones with copied
  name and ones with long lived name. This class corresponds to using the long
  lived name.

  _DictionaryItemWithLiteralKey generates calls on
  |base::trace_event::TracedValue| which do not copy the name (assume that
  the used name is long lived. Thus self.name is used inside a "quoted" long
  lived string.

  Attributes:
    value: {string} The name of the C++ variable of type
      |base::trace_event::TracedValue*| this object represents.
    name: {string} The name of the mojo variable that is currently being
      traced. Used inside double-quotes in method calls.
  """

  def __init__(self, name, value):
    super(_DictionaryItemWithLiteralKey, self).__init__(value)
    self.name = name

  def AddSingleValue(self, trace_type, parameter_value):
    """Return a line of C++ code that will set a single value to |self.value|.
    Uses |self.name| inside a "quoted" string.

    Args:
      trace_type: {string} The type of the set value. Can be one of:
        'Integer', 'Double', 'Boolean', 'String', 'Value'.
      parameter_value: {string} The C++ expression to be passed as the
        set value.

    Returns:
      A single line of C++ that sets |parameter_value| of name |"self.name"|
      on self.value.
    """
    return '%s->Set%s("%s", %s);' % (self.value, trace_type, self.name,
                                     parameter_value)

  def BeginContainer(self, container_type):
    return '%s->Begin%s("%s");' % (self.value, container_type, self.name)


class _DictionaryItemWithCopiedKey(_OutputContext):
  """Represents a |TracedValue| which expects to receive a dictionary item
  (with a name which is a temporary string). This means that
  |AddSingleValue| will return a call on self.value which is an
  SetXWithCopiedName and |BeginContainer| will start a container with
  self.name used as a temporary string.

  |base::trace_event::TracedValue| has two sets of methods -- ones with copied
  name and ones with long lived name. This class corresponds to using the
  copied name.

  _DictionaryItemWithCopiedKey generates calls on
  |base::trace_event::TracedValue| which do copy the name (assume that
  self.name is a C++ expression evaluating to a temporary string).

  Attributes:
    value: {string} The name of the C++ variable of type
      |base::trace_event::TracedValue*| this object represents.
    name: {string} The name of the mojo variable that is currently being
      traced. Used directly (not in double quotes) in method calls.
  """

  def __init__(self, name, value):
    super(_DictionaryItemWithCopiedKey, self).__init__(value)
    self.name = name

  def AddSingleValue(self, trace_type, parameter_value):
    """Return a line of C++ code that will set a single value to |self.value|.
    Uses |self.name| directly as a parameter that is expected to be copied
    (can be a temporary string).

    Args:
      trace_type: {string} The type of the set value. Can be one of:
        'Integer', 'Double', 'Boolean', 'String', 'Value'.
      parameter_value: {string} The C++ expression to be passed as the
        set value.

    Returns:
      A single line of C++ that sets |parameter_value| of name |self.name|
      on self.value (with copied name).
    """
    return '%s->Set%sWithCopiedName(%s, %s);' % (self.value, trace_type,
                                                 self.name, parameter_value)

  def BeginContainer(self, container_type):
    return '%s->Begin%sWithCopiedName(%s);' % (self.value, container_type,
                                               self.name)


def _WriteInputParamForTracingImpl(generator, kind, cpp_parameter_name,
                                   output_context):
  """Generates lines of C++ to log a parameter into TracedValue
  |output_context.value|. Use |output_context.name| if |output_context| is of
  inhereted type from _OutputContext.

  Args:
    kind: {Kind} The kind of the parameter (corresponds to its C++ type).
    cpp_parameter_name: {string} The actual C++ variable name corresponding to
      the mojom parameter |parameter_name|. Can be a valid C++ expression
      (e.g., dereferenced variable |"(*var)"|).
    output_context: {_OutputContext} Represents the TracedValue* variable to be
      written into. Possibly also holds the mojo parameter name corresponding to
      |cpp_parameter_name|.

  Yields:
    {string} C++ lines of code that trace a |cpp_parameter_name| into
      |output_context.value|.
  """

  def _WrapIfNullable(inner_lines):
    """Check if kind is nullable if so yield code to check whether it has
    value.

    Args:
      inner_lines: {function} Function taking single argument and returning
        iterable. If kind is nullable, yield from this method with
        |cpp_parameter_name+'.value()'| otherwise yield with the parameter
        |cpp_parameter_name|.

    Args from the surrounding method:
      kind
      cpp_parameter_name
      output_context.AddSingleValue
    """
    if mojom.IsNullableKind(kind):
      yield 'if (%s.has_value()) {' % cpp_parameter_name
      for line in inner_lines(cpp_parameter_name + '.value()'):
        yield '  ' + line
      yield '} else {'
      yield '  ' + output_context.AddSingleValue('String', '"base::nullopt"')
      yield '}'
    else:
      # |yield from| is introduced in Python3.3.
      for line in inner_lines(cpp_parameter_name):
        yield line

  if mojom.IsEnumKind(kind):
    if generator._IsTypemappedKind(kind) or IsNativeOnlyKind(kind):
      yield output_context.AddSingleValue(
          'Integer', 'static_cast<int>(%s)' % cpp_parameter_name)
    else:
      yield output_context.AddSingleValue(
          'String', 'base::trace_event::ValueToString(%s)' % cpp_parameter_name)
    return
  if mojom.IsStringKind(kind):
    if generator.for_blink:
      # WTF::String is nullable on its own.
      yield output_context.AddSingleValue('String',
                                          '%s.Utf8()' % cpp_parameter_name)
      return
    # The type might be base::Optional<std::string> or std::string.
    for line in _WrapIfNullable(lambda cpp_parameter_name: [
        output_context.AddSingleValue('String', cpp_parameter_name)
    ]):
      yield line
    return
  if kind == mojom.BOOL:
    yield output_context.AddSingleValue('Boolean', cpp_parameter_name)
    return
  # TODO(crbug.com/1103623): Make TracedValue support int64_t, then move to
  # mojom.IsIntegralKind.
  if kind in [mojom.INT8, mojom.UINT8, mojom.INT16, mojom.UINT16, mojom.INT32]:
    # Parameter is representable as 32bit int.
    yield output_context.AddSingleValue('Integer', cpp_parameter_name)
    return
  if kind in [mojom.UINT32, mojom.INT64, mojom.UINT64]:
    yield output_context.AddSingleValue(
        'String', 'base::NumberToString(%s)' % cpp_parameter_name)
    return
  if mojom.IsFloatKind(kind) or mojom.IsDoubleKind(kind):
    yield output_context.AddSingleValue('Double', cpp_parameter_name)
    return
  if (mojom.IsStructKind(kind) and not generator._IsTypemappedKind(kind)
      and not IsNativeOnlyKind(kind)):
    yield 'if (%s.is_null()) {' % cpp_parameter_name
    yield '  ' + output_context.AddSingleValue('String', '"nullptr"')
    yield '} else {'
    yield '  ' + output_context.BeginContainer('Dictionary')
    yield '  ' + output_context.AsValueInto(cpp_parameter_name)
    yield '  ' + output_context.EndContainer('Dictionary')
    yield '}'
    return

  if mojom.IsArrayKind(kind):
    iterator_name = 'item'
    loop_body = _WriteInputParamForTracingImpl(generator=generator,
                                               kind=kind.kind,
                                               cpp_parameter_name=iterator_name,
                                               output_context=_ArrayItem(
                                                   output_context.value))
    loop_generator = lambda cpp_parameter_name: output_context.TraceContainer(
        container_type='Array',
        iterator_name=iterator_name,
        container_name=cpp_parameter_name,
        loop_body=loop_body)
    # Array might be a nullable kind.
    for line in _WrapIfNullable(loop_generator):
      yield line
    return

  def _TraceEventToString(cpp_parameter_name=cpp_parameter_name, kind=kind):
    return 'base::trace_event::ValueToString(%s, "<value of type %s>")' % (
        cpp_parameter_name, generator._GetCppWrapperParamType(kind))

  if mojom.IsMapKind(kind):
    iterator_name = 'item'
    if generator.for_blink:
      # WTF::HashMap<,>
      key_access = '.key'
      value_access = '.value'
    else:
      # base::flat_map<,>
      key_access = '.first'
      value_access = '.second'
    loop_body = _WriteInputParamForTracingImpl(
        generator=generator,
        kind=kind.value_kind,
        cpp_parameter_name=iterator_name + value_access,
        output_context=_DictionaryItemWithCopiedKey(
            value=output_context.value,
            name=_TraceEventToString(cpp_parameter_name=iterator_name +
                                     key_access,
                                     kind=kind.key_kind)))
    loop_generator = lambda cpp_parameter_name: output_context.TraceContainer(
        container_type="Dictionary",
        iterator_name=iterator_name,
        container_name=cpp_parameter_name,
        loop_body=loop_body)
    # Dictionary might be a nullable kind.
    for line in _WrapIfNullable(loop_generator):
      yield line
    return
  if (mojom.IsInterfaceRequestKind(kind)
      or mojom.IsAssociatedInterfaceRequestKind(kind)):
    yield output_context.AddSingleValue('Boolean',
                                        cpp_parameter_name + '.is_pending()')
    return
  if (mojom.IsAnyHandleOrInterfaceKind(kind)
      and not mojom.IsInterfaceKind(kind)):
    yield output_context.AddSingleValue('Boolean',
                                        cpp_parameter_name + '.is_valid()')
    return
  """ The case |mojom.IsInterfaceKind(kind)| is not covered.
  |mojom.IsInterfaceKind(kind) == True| for the following types:
  |mojo::InterfacePtrInfo|, |mojo::InterfacePtr|.
    There is |mojo::InterfacePtrInfo::is_valid|,
      but not |mojo::InterfacePtrInfo::is_bound|.
    There is |mojo::InterfacePtr::is_bound|,
      but not |mojo::InterfacePtr::is_valid|.

  Both |mojo::InterfacePtrInfo| and |mojo::InterfacePtr| are deprecated.
  """
  yield output_context.AddSingleValue('String', _TraceEventToString())


def WriteInputParamForTracing(generator, kind, parameter_name,
                              cpp_parameter_name, value):
  """Generates lines of C++ to log parameter |parameter_name| into TracedValue
  |value|.

  Args:
    kind: {Kind} The kind of the parameter (corresponds to its C++ type).
    cpp_parameter_name: {string} The actual C++ variable name corresponding to
      the mojom parameter |parameter_name|. Can be a valid C++ expression
      (e.g., dereferenced variable |"(*var)"|).
    value: {string} The C++ |TracedValue*| variable name to be logged into.

  Yields:
    {string} C++ lines of code that trace |parameter_name| into |value|.
  """
  for line in _WriteInputParamForTracingImpl(
      generator=generator,
      kind=kind,
      cpp_parameter_name=cpp_parameter_name,
      output_context=_DictionaryItemWithLiteralKey(name=parameter_name,
                                                   value=value)):
    yield line

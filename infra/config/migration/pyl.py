# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module for parsing pyl files, retaining comments.

pyl files are files containing a single python literal value with
optional comments. If the underlying value is desired, ast.literal_eval
can be called with the contents of the file. For transformations such as
rewriting or converting to another format, this would be insufficient
since that would discard comments present in the file. This module
provides the means to introspect on the value contained in the file
while preserving comments.

The parse function provides a means of turning a pyl file into a series
of Node objects. The Node objects are similar to ast.Node. Because pyl
files contain a limited subset of python syntax, there are far fewer
leaf node types (Constant, Dict, List) than provided by the ast module.

In addition to the node types for values, the Comment subclass of Node
is provided to represent comments within the source. Comments are
exposed in the following ways:
* Comments where the preceding token is a newline or at the start of a
  stream:
  * Comments at file scope will be returned as separate elements from
    parse. The comments attribute of the Node for top-level literal will
    always be empty.
  * Comments within a list that precede an element will be surfaced via
    the comments attribute of the element's Value.
  * Comments within a dict that precede an item will be surfaced via the
    comments attribute of the item's key.
  * Comments within a list/dict that appear after all elements/items
    will be surfaced via the trailing_comments attribute of the
    List/Dict.
* Comments where the preceding token is not a newline:
  * Comments on a line where the preceding token is a constant (ignoring
    any comma that may follow a list element or dict item) will be
    surfaced via the end_of_line_comment attribute of the Constant.
  * Comments where the preceding token is an open bracket will be
    surfaced via the opening_comment attribute of the List/Dict the
    bracket belongs to.
  * Comments where the preceding token is a closing bracket will be
    surfaced via the end_of_line_comment attribute of the List/Dict
    that the bracket belongs to.

If parsing fails, an UnhandledParseError will be raised. It will be
checked that the file can be evaluated as a literal so this error most
likely indicates a limitation in this module rather than an actual
problem with the file.

LIMITATIONS

This module is tailored to process the existing pyl files used in the
testing/buildbot directories, so it does not attempt to be fully generic
or apply additional semantic meaning to comment placement.

A not-necessarily exhaustive list of valid python literal syntax that
isn't currently supported:
* tuples
* dict items where the colon and/or start of the value aren't on the
  same line as the end of the key
* commas following a list element or dict item that aren't the same line
  as the list element or the end of the dict item's value

A not-necessarily exhaustive list of semantics that a human might
reasonably infer based on comment placement that this module will not
infer:
* Using blank lines in between whole-line comments to separate comments
  that are not associated with a following value
* Indenting a whole-line comment to line up with an end-of-line comment
  to group them
"""

import ast
import collections.abc
import dataclasses
import io
import os
import tokenize
import typing


class UnhandledParseError(Exception):
  """An error raised during parsing."""
  pass


@dataclasses.dataclass(frozen=True, kw_only=True)
class Loc:
  """A location within a file."""

  file: str | os.PathLike
  """The file being indexed into."""
  line: int
  """The 1-based line number."""
  column: int
  """The 0-based column number."""

  def __str__(self):
    return f'{self.file}:{self.line}:{self.column}'


@dataclasses.dataclass(frozen=True, kw_only=True)
class Node:
  """A source code element (value or comment) parsed from a pyl file."""

  start: Loc
  """The starting location of the element."""
  end: Loc
  """The ending location of the element."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class Comment(Node):
  """A comment parsed from a pyl file.

  The comment contains only a single line and includes the leading #
  character.
  """

  comment: str
  """The comment text."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class _ValueBase(Node):
  """A value (constant, dict or list) parsed from a pyl file."""

  comments: tuple[Comment, ...] = ()
  """Any comments that immediately precede the value."""
  end_of_line_comment: Comment | None = None
  """An end-of-line comment following the value."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class Str(_ValueBase):
  """A string parsed from a pyl file."""

  value: str
  """The evaluated string value."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class Int(_ValueBase):
  """An integer parsed from a pyl file."""

  value: int
  """The evaluated integer value."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class Bool(_ValueBase):
  """A boolean parsed from a pyl file."""

  value: bool
  """The evaluated boolean value."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class None_(_ValueBase):
  """A None value parsed from a pyl file."""

  value: None
  """The evaluated None value."""


Constant: typing.TypeAlias = Str | Int | Bool | None_

K = typing.TypeVar('K', bound=Constant)
V = typing.TypeVar('V', bound=_ValueBase)


@dataclasses.dataclass(frozen=True, kw_only=True)
class Dict(_ValueBase, typing.Generic[K, V]):
  """A dictionary parsed from a pyl file."""

  items: tuple[tuple[K, V], ...]
  """The dictionary items as tuples: (key, value)."""
  opening_comment: Comment | None = None
  """An end-of-line comment following the opening bracket."""
  trailing_comments: tuple[Comment, ...] = ()
  """Comments following the last item in the dictionary."""


E = typing.TypeVar('E', bound=_ValueBase)


@dataclasses.dataclass(frozen=True, kw_only=True)
class List(_ValueBase, typing.Generic[E]):
  """A list parsed from a pyl file."""

  elements: tuple[E, ...]
  """The list elements."""
  opening_comment: Comment | None = None
  """An end-of-line comment following the opening bracket."""
  trailing_comments: tuple[Comment, ...] = ()
  """Comments following the last element in the list."""


Value = Constant | Dict | List

_TokenType: typing.TypeAlias = int
"""The type of a tokenize.TokenInfo."""


@dataclasses.dataclass(frozen=True, kw_only=True)
class _Token:
  """A token retrieved from a pyl file.

  The type attribute of tokenize.TokenInfo objects uses include many
  separate tokens in one type (e.g. all of []{}: have type OP). The
  exact_type attribute provides a value that identifies the specific
  token. Since we need to operate on the more specific type, _Token
  provides a representation of a TokenInfo that only exposes the exact
  type to avoid potential misuse.
  """

  type: _TokenType
  """The token's type."""
  start: Loc
  """The token's starting location."""
  end: Loc
  """The token's ending location."""
  string: str
  """The text of the token."""
  line: str
  """The full line containing the token."""

  @property
  def type_name(self) -> str:
    """A human-readable representation of the token's type."""
    return tokenize.tok_name[self.type]

  def __repr__(self):
    # Override the default representation to use the human-readable
    # representation of the token's type
    return f'{self.__class__.__name__}(type=tokenize.{self.type_name}, start={self.start!r}, end={self.end!r}, string={self.string!r}, line={self.line!r})'


class _TokenStream:
  """A stream of tokens from a pyl file.

  _TokenStream provides the means to access the tokens of a pyl file in
  a sequential order. The current method provides the current token of
  the stream and the advance method advances the stream to the next
  token. The final token of the stream will always have type
  tokenize.ENDMARKER.

  The require and token_error_message methods provide additional
  functionality for interacting with the current token. The consume and
  consume_if methods provide functionality for advancing the stream
  after examining the current token.
  """

  @classmethod
  def create(cls, file: str | os.PathLike, source: str) -> typing.Self:
    """Create a token stream from a file's source.

    Args:
      file: The file that the source comes from.
      source: The contents of the file.
    """
    return cls(file, tokenize.generate_tokens(io.StringIO(source).readline))

  def __init__(
      self,
      file: str | os.PathLike,
      tokens: collections.abc.Iterable[tokenize.TokenInfo],
  ):
    self._file = file
    self._tokens = map(self._token, tokens)
    self._current_token = next(self._tokens)

  def _token(self, token: tokenize.TokenInfo) -> _Token:
    return _Token(
        type=token.exact_type,
        start=Loc(file=self._file, line=token.start[0], column=token.start[1]),
        end=Loc(file=self._file, line=token.end[0], column=token.end[1]),
        string=token.string,
        line=token.line,
    )

  def current(self) -> _Token:
    """The current token of the stream.

    When the stream is created, the current token will be the first
    token in the file. The stream's final token will always have type
    tokenize.ENDMARKER, after which it is an error to try and advance
    the stream.
    """
    return self._current_token

  def advance(self) -> None:
    """Advance the stream to the next token.

    Raises:
      ValueError: If there are no more tokens available
        (self.current().type == tokenize.ENDMARKER).
    """
    try:
      self._current_token = next(self._tokens)
      return
    except StopIteration:
      # Don't raise the ValueError in the except block to avoid the implicit
      # exception chaining
      pass
    raise ValueError('no more tokens available')

  def token_error_message(self, message: str) -> str:
    """Get an error message including the location for the current token.

    The message will include additional information about the current
    token: the token's type as well as the actual text of the line
    including the token with markers to highlight the token.

    Args:
      message: The custom message to include in the error message.

    Returns:
      The error message.
    """
    start = self._current_token.start
    end = self._current_token.end
    return (f'{start}: {message}, got {self._current_token.type_name}\n'
            f'{self._current_token.line}\n'
            f'{" " * (start.column)}{"^" * (end.column - start.column)}')

  def require(
      self,
      token_type: _TokenType | collections.abc.Collection[_TokenType],
  ) -> None:
    """Enforce the current token's type.

    Args:
      token_type: The expected token type(s). If a collection is
        provided, the current token must have one of the provided types.

    Raises:
      UnhandledParseError: If the current token is not of the expected
        type(s).
    """
    if isinstance(token_type, collections.abc.Collection):
      if self._current_token.type in token_type:
        return
      token_type_names = [tokenize.tok_name[t] for t in token_type]
      message = f'expected token of one of types {", ".join(token_type_names[:-1])} or {token_type_names[-1]}'
    else:
      if self._current_token.type == token_type:
        return
      message = f'expected token of type {tokenize.tok_name[token_type]}'

    message = self.token_error_message(message)
    raise UnhandledParseError(message)

  def consume(
      self,
      token_type: _TokenType | collections.abc.Collection[_TokenType],
  ) -> _Token:
    """Enforce the current token's type and advance the stream.

    If the current token has the expected type, then it will be returned
    and the stream will be advanced.

    Args:
      token_type: The expected token type(s).

    Returns:
      The current token before the stream was advanced.

    Raises:
      UnhandledParseError: If the current token is not of the expected type.
    """
    self.require(token_type)
    ret = self._current_token
    self.advance()
    return ret

  def consume_if(
      self,
      token_type: _TokenType | collections.abc.Collection[_TokenType],
  ) -> _Token | None:
    """Advance the stream if the current token has expected type(s).

    Args:
      token_type: The expected token type(s).

    Returns:
      The current token before the stream was advanced if the current
      token was of the expected type(s). Otherwise, None.
    """
    try:
      return self.consume(token_type)
    except UnhandledParseError:
      return None


# IMPLEMENTATION NOTE:
#
# Functions starting with _parse that instantiate leaf Node types should take
# the token_stream as their first argument. They should expect the stream's
# current token to be the first token of the node to be parsed. They are
# responsible for advancing the the stream past the last token token of the node
# to be parsed. For functions instantiating Value types, they should not try to
# handle any preceding or following comments, _parse_value will take in any
# preceding comments that its caller wishes to associate with the value and it
# will add any end-of-line comment that directly follows the value.


def _parse_comment(token_stream: _TokenStream) -> Comment:
  comment_token = token_stream.consume(tokenize.COMMENT)
  return Comment(
      comment=comment_token.string,
      start=comment_token.start,
      end=comment_token.end,
  )


def _add_end_of_line_comment_if_present(
    token_stream: _TokenStream,
    value: Value,
) -> Value:
  if token_stream.current().type != tokenize.COMMENT:
    return value
  return dataclasses.replace(value,
                             end_of_line_comment=_parse_comment(token_stream))


_CONSTANT_TOKEN_TYPES = (tokenize.STRING, tokenize.NUMBER, tokenize.NAME)
_ALLOWED_NAMES = ('None', 'True', 'False')


def _parse_constant(token_stream: _TokenStream) -> Constant:
  token = token_stream.consume(_CONSTANT_TOKEN_TYPES)
  match token.type:
    case tokenize.STRING:
      return Str(
          value=ast.literal_eval(token.string),
          start=token.start,
          end=token.end,
      )
    case tokenize.NUMBER:
      return Int(
          value=ast.literal_eval(token.string),
          start=token.start,
          end=token.end,
      )
    case tokenize.NAME:
      assert token.string in _ALLOWED_NAMES, f'got unexpected name {token.string}'
      if token.string == 'None':
        return None_(
            value=None,
            start=token.start,
            end=token.end,
        )
      return Bool(
          value=token.string == 'True',
          start=token.start,
          end=token.end,
      )
  assert False, "unreachable"  # pragma: no cover


def _parse_dict(token_stream: _TokenStream) -> Dict:
  lbrace_token = token_stream.consume(tokenize.LBRACE)

  opening_comment = None
  if token_stream.current().type == tokenize.COMMENT:
    opening_comment = _parse_comment(token_stream)

  items = []
  item_comments = []

  while True:
    token = token_stream.current()
    match token.type:
      case tokenize.NL:
        token_stream.advance()

      case tokenize.COMMENT:
        item_comments.append(_parse_comment(token_stream))

      case tokenize.RBRACE:
        token_stream.advance()
        return Dict(
            items=tuple(items),
            start=lbrace_token.start,
            end=token.end,
            opening_comment=opening_comment,
            trailing_comments=tuple(item_comments),
        )

      case _ if token.type in _CONSTANT_TOKEN_TYPES:
        key = _parse_value(token_stream, comments=item_comments)
        item_comments = []
        token_stream.consume(tokenize.COLON)
        value = _parse_value(token_stream)
        token_stream.consume_if(tokenize.COMMA)
        value = _add_end_of_line_comment_if_present(token_stream, value)
        items.append((key, value))

      case _:
        message = token_stream.token_error_message(
            'unexpected token type while parsing dict')
        raise UnhandledParseError(message)


def _parse_list(token_stream: _TokenStream) -> List:
  lsqb_token = token_stream.consume(tokenize.LSQB)

  opening_comment = None
  if token_stream.current().type == tokenize.COMMENT:
    opening_comment = _parse_comment(token_stream)

  elements = []
  element_comments = []

  while True:
    token = token_stream.current()
    match token.type:
      case tokenize.NL:
        token_stream.advance()

      case tokenize.COMMENT:
        element_comments.append(_parse_comment(token_stream))

      case tokenize.RSQB:
        token_stream.advance()
        return List(
            elements=tuple(elements),
            start=lsqb_token.start,
            end=token.end,
            opening_comment=opening_comment,
            trailing_comments=tuple(element_comments),
        )

      case _ if token.type in _VALUE_PARSERS:
        element = _parse_value(token_stream, comments=tuple(element_comments))
        element_comments = []
        token_stream.consume_if(tokenize.COMMA)
        element = _add_end_of_line_comment_if_present(token_stream, element)
        elements.append(element)

      case _:
        message = token_stream.token_error_message(
            'unexpected token type while parsing list')
        raise UnhandledParseError(message)


_VALUE_PARSERS = {
    tokenize.LBRACE: _parse_dict,
    tokenize.LSQB: _parse_list,
} | {
    token_type: _parse_constant
    for token_type in _CONSTANT_TOKEN_TYPES
}


def _parse_value(
    token_stream: _TokenStream,
    *,
    comments: collections.abc.Iterable[Comment] = (),
) -> Value:
  token_stream.require(_VALUE_PARSERS.keys())
  value_parser = _VALUE_PARSERS[token_stream.current().type]
  value = value_parser(token_stream)
  if comments:
    value = dataclasses.replace(value, comments=tuple(comments))
  return _add_end_of_line_comment_if_present(token_stream, value)


def _parse_file(token_stream: _TokenStream) -> collections.abc.Iterable[Node]:
  while True:
    token = token_stream.current()
    match token.type:
      case tokenize.ENDMARKER:
        break

      case tokenize.NL | tokenize.NEWLINE:
        token_stream.advance()

      case tokenize.COMMENT:
        yield _parse_comment(token_stream)

      case _ if token.type in _VALUE_PARSERS:
        yield _parse_value(token_stream)

      case _:
        message = token_stream.token_error_message(
            'unexpected token type at file scope')
        raise UnhandledParseError(message)


def parse(
    file: str | os.PathLike,
    source: str,
) -> collections.abc.Iterable[Node]:
  """Parse a pyl file.

  Args:
    file: The file being parsed. source: The contents of the file.

  Returns:
    An iterable of Nodes. Because pyl files must contain a single
    literal value, the iterable will contain exactly one element that is
    a Value instance and all other elements will be Comment.

  Raises:
    ValueError: If the file does not contain exactly one literal.
    UnhandledParseError: If the file cannot be parsed by this module.
      This will only be raised if the file can already be successfully
      evaluated, so it represents a limitation of this module rather
      than a problem with the file.
  """
  # Make sure that the source contains a literal: once the content is verified
  # to contain a valid python literal, it's tractable to decompose the tokens
  # into ast-like types that store comments
  try:
    ast.literal_eval(source)
  except:
    raise ValueError(f'{file} does not contain exactly one literal')

  token_stream = _TokenStream.create(file, source)
  return _parse_file(token_stream)

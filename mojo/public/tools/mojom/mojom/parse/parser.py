# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates a syntax tree from a Mojo IDL file."""

# Breaking parser stanzas is unhelpful so allow longer lines.
# pylint: disable=line-too-long

import os.path
import sys

from mojom import fileutil
from mojom.error import Error
from mojom.parse import ast
from mojom.parse.lexer import Lexer

fileutil.AddLocalRepoThirdPartyDirToModulePath()
from ply import lex
from ply import yacc

_MAX_ORDINAL_VALUE = 0xffffffff
_MAX_ARRAY_SIZE = 0xffffffff


class ParseError(Error):
  """Class for errors from the parser."""

  def __init__(self, filename, message, lineno=None, snippet=None):
    Error.__init__(
        self,
        filename,
        message,
        lineno=lineno,
        addenda=([snippet] if snippet else None))


# We have methods which look like they could be functions:
# pylint: disable=R0201
class Parser:
  def __init__(self, lexer, source, filename):
    self.tokens = lexer.tokens
    self.source = source
    self.filename = filename

  def _set_lexstate(self, p, start_token, end_token):
    """Sets the lexer state in `p[0]` to track line and input stream positions.
    If `start_token` or `end_token` are AST nodes rather than LexTokens, this
    function uses the values from the node.

    Args:
      p: The YaccProduction for the parse function. `p[0]` is the output that
          will be modified.
      start_token: The index in `p` to use for the starting line and lex
          positions.
      end_token: The index in `p` to use for the ending line and lex positions.
    """
    p[0].filename = self.filename

    p[0].start = ast.Location(p.lineno(start_token), p.lexpos(start_token))
    if not p[0].start.lexpos and isinstance(p[start_token], ast.NodeBase):
      p[0].start = p[start_token].start

    lexpos = p.lexpos(end_token)
    if lexpos:
      p[0].end = ast.Location(p.lineno(end_token), lexpos + len(p[end_token]))
    elif isinstance(p[end_token], ast.NodeBase):
      p[0].end = p[end_token].end

  # Names of functions
  #
  # In general, we name functions after the left-hand-side of the rule(s) that
  # they handle. E.g., |p_foo_bar| for a rule |foo_bar : ...|.
  #
  # There may be multiple functions handling rules for the same left-hand-side;
  # then we name the functions |p_foo_bar_N| (for left-hand-side |foo_bar|),
  # where N is a number (numbered starting from 1). Note that using multiple
  # functions is actually more efficient than having single functions handle
  # multiple rules (and, e.g., distinguishing them by examining |len(p)|).
  #
  # It's also possible to have a function handling multiple rules with different
  # left-hand-sides. We do not do this.
  #
  # See http://www.dabeaz.com/ply/ply.html#ply_nn25 for more details.

  # TODO(vtl): Get rid of the braces in the module "statement". (Consider
  # renaming "module" -> "package".) Then we'll be able to have a single rule
  # for root (by making module "optional").
  def p_root_1(self, p):
    """root : """
    p[0] = ast.Mojom(None, ast.ImportList(), [])

  def p_root_2(self, p):
    """root : root module"""
    if p[1].module is not None:
      raise ParseError(self.filename,
                       "Multiple \"module\" statements not allowed:",
                       p[2].start.line,
                       snippet=self._GetSnippet(p[2].start.line))
    if p[1].import_list.items or p[1].definition_list:
      raise ParseError(
          self.filename,
          "\"module\" statements must precede imports and definitions:",
          p[2].start.line,
          snippet=self._GetSnippet(p[2].start.line))
    p[0] = p[1]
    p[0].module = p[2]

  def p_root_3(self, p):
    """root : root import"""
    if p[1].definition_list:
      raise ParseError(self.filename,
                       "\"import\" statements must precede definitions:",
                       p[2].start.line,
                       snippet=self._GetSnippet(p[2].start.line))
    p[0] = p[1]
    p[0].import_list.Append(p[2])

  def p_root_4(self, p):
    """root : root definition"""
    p[0] = p[1]
    p[0].definition_list.append(p[2])

  def p_import(self, p):
    """import : attribute_section IMPORT STRING_LITERAL SEMI"""
    # 'eval' the literal to strip the quotes.
    # TODO(vtl): This eval is dubious. We should unquote/unescape ourselves.
    p[0] = ast.Import(p[1], eval(p[3]))
    self._set_lexstate(p, 2, 4)

  def p_module(self, p):
    """module : attribute_section MODULE identifier SEMI"""
    p[0] = ast.Module(p[3], p[1])
    self._set_lexstate(p, 2, 4)

  def p_definition(self, p):
    """definition : struct
                  | union
                  | interface
                  | enum
                  | const
                  | feature"""
    p[0] = p[1]

  def p_attribute_section_1(self, p):
    """attribute_section : """
    p[0] = None

  def p_attribute_section_2(self, p):
    """attribute_section : LBRACKET attribute_list RBRACKET"""
    p[0] = p[2]
    self._set_lexstate(p, 1, 3)

  def p_attribute_list_1(self, p):
    """attribute_list : """
    p[0] = ast.AttributeList()

  def p_attribute_list_2(self, p):
    """attribute_list : nonempty_attribute_list"""
    p[0] = p[1]

  def p_nonempty_attribute_list_1(self, p):
    """nonempty_attribute_list : attribute"""
    p[0] = ast.AttributeList(p[1])

  def p_nonempty_attribute_list_2(self, p):
    """nonempty_attribute_list : nonempty_attribute_list COMMA attribute"""
    p[0] = p[1]
    p[0].Append(p[3])

  def p_attribute_1(self, p):
    """attribute : name EQUALS identifier"""
    p[0] = ast.Attribute(p[1], p[3])
    self._set_lexstate(p, 1, 3)

  def p_attribute_2(self, p):
    """attribute : name EQUALS evaled_literal
                 | name EQUALS name"""
    p[0] = ast.Attribute(p[1], p[3])
    self._set_lexstate(p, 1, 3)

  def p_attribute_3(self, p):
    """attribute : name"""
    p[0] = ast.Attribute(p[1], True)
    self._set_lexstate(p, 1, 1)

  def p_evaled_literal(self, p):
    """evaled_literal : literal"""
    # 'eval' the literal to strip the quotes. Handle keywords "true" and "false"
    # specially since they cannot directly be evaluated to python boolean
    # values.
    if p[1] == "true":
      p[0] = True
    elif p[1] == "false":
      p[0] = False
    else:
      p[0] = eval(p[1].value)

  def p_struct_1(self, p):
    """struct : attribute_section STRUCT name LBRACE struct_body RBRACE SEMI"""
    p[0] = ast.Struct(p[3], p[1], p[5])
    self._set_lexstate(p, 2, 7)

  def p_struct_2(self, p):
    """struct : attribute_section STRUCT name SEMI"""
    p[0] = ast.Struct(p[3], p[1], None)
    self._set_lexstate(p, 2, 4)

  def p_struct_body_1(self, p):
    """struct_body : """
    p[0] = ast.StructBody()

  def p_struct_body_2(self, p):
    """struct_body : struct_body const
                   | struct_body enum
                   | struct_body struct_field"""
    p[0] = p[1]
    p[0].Append(p[2])

  def p_struct_field(self, p):
    """struct_field : attribute_section typename name ordinal default SEMI"""
    p[0] = ast.StructField(p[3], p[1], p[4], p[2], p[5])
    self._set_lexstate(p, 2, 6)

  def p_feature(self, p):
    """feature : attribute_section FEATURE name LBRACE feature_body RBRACE SEMI"""
    p[0] = ast.Feature(p[3], p[1], p[5])
    self._set_lexstate(p, 2, 7)

  def p_feature_body_1(self, p):
    """feature_body : """
    p[0] = ast.FeatureBody()

  def p_feature_body_2(self, p):
    """feature_body : feature_body const"""
    p[0] = p[1]
    p[0].Append(p[2])

  def p_union(self, p):
    """union : attribute_section UNION name LBRACE union_body RBRACE SEMI"""
    p[0] = ast.Union(p[3], p[1], p[5])
    self._set_lexstate(p, 2, 7)

  def p_union_body_1(self, p):
    """union_body : """
    p[0] = ast.UnionBody()

  def p_union_body_2(self, p):
    """union_body : union_body union_field"""
    p[0] = p[1]
    p[1].Append(p[2])

  def p_union_field(self, p):
    """union_field : attribute_section typename name ordinal SEMI"""
    p[0] = ast.UnionField(p[3], p[1], p[4], p[2])
    self._set_lexstate(p, 2, 5)

  def p_default_1(self, p):
    """default : """
    p[0] = None

  def p_default_2(self, p):
    """default : EQUALS constant"""
    p[0] = p[2]

  def p_interface(self, p):
    """interface : attribute_section INTERFACE name LBRACE interface_body RBRACE SEMI"""
    p[0] = ast.Interface(p[3], p[1], p[5])
    self._set_lexstate(p, 2, 7)

  def p_interface_body_1(self, p):
    """interface_body : """
    p[0] = ast.InterfaceBody()

  def p_interface_body_2(self, p):
    """interface_body : interface_body const
                      | interface_body enum
                      | interface_body method"""
    p[0] = p[1]
    p[0].Append(p[2])

  def p_response_1(self, p):
    """response : """
    p[0] = None

  def p_response_2(self, p):
    """response : RESPONSE LPAREN parameter_list RPAREN"""
    p[0] = p[3]

  def p_method(self, p):
    """method : attribute_section name ordinal LPAREN parameter_list RPAREN response SEMI"""
    p[0] = ast.Method(p[2], p[1], p[3], p[5], p[7])
    self._set_lexstate(p, 2, 8)

  def p_parameter_list_1(self, p):
    """parameter_list : """
    p[0] = ast.ParameterList()

  def p_parameter_list_2(self, p):
    """parameter_list : nonempty_parameter_list"""
    p[0] = p[1]
    p[0].end = p[0].items[-1].end

  def p_nonempty_parameter_list_1(self, p):
    """nonempty_parameter_list : parameter"""
    p[0] = ast.ParameterList(p[1])

  def p_nonempty_parameter_list_2(self, p):
    """nonempty_parameter_list : nonempty_parameter_list COMMA parameter"""
    p[0] = p[1]
    p[0].Append(p[3])

  def p_parameter(self, p):
    """parameter : attribute_section typename name ordinal"""
    p[0] = ast.Parameter(p[3], p[1], p[4], p[2])
    self._set_lexstate(p, 2, 3)

  def p_typename(self, p):
    """typename : nonnullable_typename QSTN
                | nonnullable_typename"""
    p[0] = ast.Typename(p[1], nullable=len(p) != 2)
    self._set_lexstate(p, 1, 1)

  def p_nonnullable_typename(self, p):
    """nonnullable_typename : basictypename
                            | array
                            | fixed_array
                            | associative_array"""
    p[0] = p[1]

  def p_basictypename(self, p):
    """basictypename : remotetype
                     | receivertype
                     | associatedremotetype
                     | associatedreceivertype
                     | identifier
                     | handletype"""
    p[0] = p[1]

  def p_remotetype(self, p):
    """remotetype : PENDING_REMOTE LANGLE identifier RANGLE"""
    p[0] = ast.Remote(p[3])
    self._set_lexstate(p, 1, 4)

  def p_receivertype(self, p):
    """receivertype : PENDING_RECEIVER LANGLE identifier RANGLE"""
    p[0] = ast.Receiver(p[3])
    self._set_lexstate(p, 1, 4)

  def p_associatedremotetype(self, p):
    """associatedremotetype : PENDING_ASSOCIATED_REMOTE LANGLE identifier RANGLE"""
    p[0] = ast.Remote(p[3], associated=True)
    self._set_lexstate(p, 1, 4)

  def p_associatedreceivertype(self, p):
    """associatedreceivertype : PENDING_ASSOCIATED_RECEIVER LANGLE identifier RANGLE"""
    p[0] = ast.Receiver(p[3], associated=True)
    self._set_lexstate(p, 1, 4)

  def p_handletype(self, p):
    """handletype : HANDLE
                  | HANDLE LANGLE name RANGLE"""
    if len(p) == 2:
      p[0] = ast.Identifier(p[1])
      self._set_lexstate(p, 1, 1)
    else:
      if p[3].name not in ('data_pipe_consumer', 'data_pipe_producer',
                           'message_pipe', 'shared_buffer', 'platform'):
        raise ParseError(self.filename,
                         "Invalid handle type %r:" % p[3].name,
                         lineno=p[3].start.line,
                         snippet=self._GetSnippet(p.lineno(1)))
      p[0] = ast.Identifier(f"handle<{p[3]}>")
      self._set_lexstate(p, 1, 4)

  def p_array(self, p):
    """array : ARRAY LANGLE typename RANGLE"""
    p[0] = ast.Array(p[3])
    self._set_lexstate(p, 1, 4)

  def p_fixed_array(self, p):
    """fixed_array : ARRAY LANGLE typename COMMA INT_CONST_DEC RANGLE"""
    value = int(p[5])
    if value == 0 or value > _MAX_ARRAY_SIZE:
      raise ParseError(
          self.filename,
          "Fixed array size %d invalid:" % value,
          lineno=p.lineno(5),
          snippet=self._GetSnippet(p.lineno(5)))
    p[0] = ast.Array(p[3], fixed_size=value)
    self._set_lexstate(p, 1, 6)

  def p_associative_array(self, p):
    """associative_array : MAP LANGLE identifier COMMA typename RANGLE"""
    p[0] = ast.Map(p[3], p[5])
    self._set_lexstate(p, 1, 6)

  def p_ordinal_1(self, p):
    """ordinal : """
    p[0] = None

  def p_ordinal_2(self, p):
    """ordinal : ORDINAL"""
    value = int(p[1][1:])
    if value > _MAX_ORDINAL_VALUE:
      raise ParseError(
          self.filename,
          "Ordinal value %d too large:" % value,
          lineno=p.lineno(1),
          snippet=self._GetSnippet(p.lineno(1)))
    p[0] = ast.Ordinal(value)
    self._set_lexstate(p, 1, 1)

  def p_enum_1(self, p):
    """enum : attribute_section ENUM name LBRACE enum_value_list RBRACE SEMI
            | attribute_section ENUM name LBRACE \
                    nonempty_enum_value_list COMMA RBRACE SEMI"""
    p[0] = ast.Enum(p[3], p[1], p[5])
    self._set_lexstate(p, 2, 7 if len(p) == 8 else 8)

  def p_enum_2(self, p):
    """enum : attribute_section ENUM name SEMI"""
    p[0] = ast.Enum(p[3], p[1], None)
    self._set_lexstate(p, 2, 4)

  def p_enum_value_list_1(self, p):
    """enum_value_list : """
    p[0] = ast.EnumValueList()

  def p_enum_value_list_2(self, p):
    """enum_value_list : nonempty_enum_value_list"""
    p[0] = p[1]

  def p_nonempty_enum_value_list_1(self, p):
    """nonempty_enum_value_list : enum_value"""
    p[0] = ast.EnumValueList(p[1])

  def p_nonempty_enum_value_list_2(self, p):
    """nonempty_enum_value_list : nonempty_enum_value_list COMMA enum_value"""
    p[0] = p[1]
    p[0].Append(p[3])

  def p_enum_value(self, p):
    """enum_value : attribute_section name
                  | attribute_section name EQUALS int
                  | attribute_section name EQUALS identifier"""
    p[0] = ast.EnumValue(p[2], p[1], p[4] if len(p) == 5 else None)
    self._set_lexstate(p, 2, len(p) - 1)

  def p_const(self, p):
    """const : attribute_section CONST typename name EQUALS constant SEMI"""
    p[0] = ast.Const(p[4], p[1], p[3], p[6])
    self._set_lexstate(p, 2, 7)

  def p_constant(self, p):
    """constant : literal
                | identifier"""
    p[0] = p[1]

  def p_identifier(self, p):
    """identifier : name
                  | name DOT identifier"""
    p[0] = ast.Identifier(''.join(map(str, p[1:])))
    self._set_lexstate(p, 1, len(p) - 1)

  # Allow 'feature' to be a name literal not just a keyword.
  def p_name(self, p):
    """name : NAME
            | FEATURE"""
    p[0] = ast.Name(p[1])
    self._set_lexstate(p, 1, 1)

  def p_literal(self, p):
    """literal : int
               | float
               | TRUE
               | FALSE
               | DEFAULT
               | STRING_LITERAL"""
    if isinstance(p[1], ast.Literal):
      p[0] = p[1]
    else:
      p[0] = ast.Literal(p.slice[1].type, p[1])
      self._set_lexstate(p, 1, 1)


  def p_int(self, p):
    """int : int_const
           | PLUS int_const
           | MINUS int_const"""
    p[0] = ast.Literal('int', ''.join(map(str, p[1:])))
    self._set_lexstate(p, 1, len(p) - 1)

  def p_int_const(self, p):
    """int_const : INT_CONST_DEC
                 | INT_CONST_HEX"""
    p[0] = ast.Literal('int', p[1])
    self._set_lexstate(p, 1, 1)

  def p_float(self, p):
    """float : FLOAT_CONST
             | PLUS FLOAT_CONST
             | MINUS FLOAT_CONST"""
    p[0] = ast.Literal('float', ''.join(p[1:]))
    self._set_lexstate(p, 1, len(p) - 1)

  def p_error(self, e):
    if e is None:
      # Unexpected EOF.
      # TODO(vtl): Can we figure out what's missing?
      raise ParseError(self.filename, "Unexpected end of file")

    if e.value == 'feature':
      raise ParseError(self.filename,
                       "`feature` is reserved for a future mojom keyword",
                       lineno=e.lineno,
                       snippet=self._GetSnippet(e.lineno))

    raise ParseError(
        self.filename,
        "Unexpected %r:" % e.value,
        lineno=e.lineno,
        snippet=self._GetSnippet(e.lineno))

  def _GetSnippet(self, lineno):
    return self.source.split('\n')[lineno - 1]


def Parse(source, filename, with_comments=False):
  """Parse source file to AST.

  Args:
    source: The source text as a str (Python 2 or 3) or unicode (Python 2).
    filename: The filename that |source| originates from.
    with_comments: If True, parses comments and attaches them to AST nodes.
        Otherwise, they are discarded.

  Returns:
    The AST as a mojom.parse.ast.Mojom object.
  """
  lexer = Lexer(filename)
  parser = Parser(lexer, source, filename)

  lex.lex(object=lexer)
  yacc.yacc(module=parser, debug=0, write_tables=0)

  tree = yacc.parse(source)
  if with_comments:
    _AssignComments(tree, lexer.line_comments, lexer.suffix_comments)
  return tree


def _AssignComments(tree, line_comments, suffix_comments):
  """Attaches comments to AST nodes in `tree`. Uses the algorithm described at
  https://jayconrod.com/posts/129/preserving-comments-when-parsing-and-formatting-code
  by performing two tree traversals for assigning line and suffix comments.
  """
  walker = _AstWalker()
  walker.Walk(tree)

  # The only types of nodes that may not have lexpos state.
  no_lexpos_nodes = (ast.Mojom, ast.Module, ast.ParameterList)

  # Assign comments on their own line to the node that immediately follows.
  comment_index = 0
  for node in walker.pre:
    if not node.start[0]:
      assert isinstance(node, no_lexpos_nodes), f'Missing lexpos for {node}'
      continue
    while comment_index < len(line_comments):
      comment = line_comments[comment_index]
      if node.start.lexpos > comment.lexpos:
        if isinstance(node, _BodyEnd):
          # If this is the end of an enclosing scope, then attach it to the
          # "after" comments section.
          node.node.append_comment(after=comment)
        else:
          # The comment precedes this node.
          node.append_comment(before=comment)
        comment_index += 1
      else:
        break

  # Any remaining line comments are at the end of the file.
  walker.pre[0].comments_after = line_comments[comment_index:]

  # Assign suffix comments in reverse order.
  for node in reversed(walker.post):
    if not node.start[0]:
      assert isinstance(node, no_lexpos_nodes), f'Missing lexpos for {node}'
      continue

    # Don't assign suffix comments to block nodes.
    if node.start.line != node.end.line:
      continue

    comment_index = len(suffix_comments) - 1
    while comment_index >= 0:
      comment = suffix_comments[comment_index]
      if node.start.line == comment.lineno:
        if isinstance(node, _BodyEnd):
          # Transform suffix comments on the body end to be at the end of
          # the inner enclosing node.
          node.node.append_comment(after=comment)
        else:
          node.append_comment(suffix=comment)
        del suffix_comments[comment_index]
      comment_index -= 1
    # Iterating in reverse means comments order should be
    # reversed.
    if node.comments_suffix:
      node.comments_suffix = list(reversed(node.comments_suffix))

  # Attach any remaining suffix comments to the first sub-node of the
  # ast.Mojom.
  if suffix_comments:
    for comment in suffix_comments:
      walker.post[-2].append_comment(before=comment)


class _BodyEnd(ast.NodeBase):
  """A synthetic AST node used to demarcate the end of an AST node's children.
  This can be thought of as the trailing semicolon of a struct or interface
  definition.
  """

  def __init__(self, node):
    super().__init__(filename=node.filename)
    self.start = node.end
    self.end = ast.Location(node.end.line, node.end.lexpos + 1)
    self.node = node


class _AstWalker:
  """Iterates the AST nodes to which comments should be attached in pre- and
  post-order traversal, storing the results in two lists. Nodes of type
  NodeListBase are un-rolled by the parent node. To identify the end of scopes,
  a _BodyEnd node is inserted. Not all AST nodes are emitted, if comment nodes
  should be associated with a higher AST node instead of the lower leaf.
  """

  def __init__(self):
    self.pre = []
    self.post = []

  def Walk(self, node):
    if node is None:
      return

    self.pre.append(node)

    if isinstance(node, ast.Mojom):
      self.Walk(node.module)
      for item in node.import_list:
        self.Walk(item)
      for item in node.definition_list:
        self.Walk(item)
    elif isinstance(node, ast.Module):
      pass
    elif isinstance(node, ast.Import):
      pass
    elif isinstance(node, ast.Interface):
      for item in node.body:
        self.Walk(item)
      self.Walk(_BodyEnd(node))
    elif isinstance(node, ast.Method):
      for param in node.parameter_list:
        self.Walk(param)
      if node.response_parameter_list:
        for param in node.response_parameter_list:
          self.Walk(param)
    elif isinstance(node, ast.Parameter):
      pass
    elif isinstance(node, ast.Const):
      pass
    elif isinstance(node, ast.Enum):
      if node.enum_value_list:
        for item in node.enum_value_list:
          self.Walk(item)
        self.Walk(_BodyEnd(node))
    elif isinstance(node, ast.EnumValue):
      pass
    elif isinstance(node, (ast.Struct, ast.Feature)):
      if node.body:
        for item in node.body:
          self.Walk(item)
        self.Walk(_BodyEnd(node))
    elif isinstance(node, ast.StructField):
      pass
    elif isinstance(node, ast.Union):
      for item in node.body:
        self.Walk(item)
      self.Walk(_BodyEnd(node))
    elif isinstance(node, ast.UnionField):
      pass
    elif isinstance(node, ast.AttributeList):
      for item in node:
        self.Walk(item)
    elif isinstance(node, _BodyEnd):
      pass
    else:
      raise ValueError(f'Unexpected AST node {repr(node)}')

    self.post.append(node)

# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Implementation of the Mojom formatter.

This operates over the AST and re-formats the entire Mojom into a standard
style. The general style is:

- Wrap lines to 80 columns, base indent is 2, and continuation lines are
  indented by 4.
- New lines are inserted between major declaration blocks.
- Preserve significant newline whitespace to allow grouping declarations.
- Always insert newlines between methods to encourage comments.

Internally, there are two kinds of function invariants:
  - _write_X() functions accept an AST node and a FormatState and directly
     write to the final output buffer.
  - _format_X() functions accept an input to format and return the formatted
    output so it can be composed.
"""

from io import StringIO
import sys

from mojom.parse import ast, parser

# The maximum line length.
LINE_LENGTH = 80
# The number of spaces of a standard indent.
INDENT_SHIFT = 2
# The number of spaces to indent a wrapped continuation line.
CONTINUATION_SHIFT = 4


def mojom_format(filename, contents=None):
    """Formats the Mojom file at `filename` and returns the formatted output.
    If `contents` is provided, then it is used instead of reading the file.
    """
    if contents is None:
        with open(filename) as f:
            contents = f.read()
    tree = parser.Parse(contents, filename, with_comments=True)
    output = StringIO()
    state = FormatState(output)
    _write_mojom(tree, state)
    return output.getvalue()


def _write_mojom(node, state):
    assert isinstance(node, ast.Mojom)

    if node.module:
        _write_module(node.module, state)
    _write_import_list(node.import_list.items, state)

    for i, defn in enumerate(node.definition_list):
        if isinstance(defn, ast.Interface):
            _write_body('interface', defn, state)
        elif isinstance(defn, ast.Struct):
            _write_body('struct', defn, state)
        elif isinstance(defn, ast.Enum):
            _write_body('enum', defn, state, bodyattr='enum_value_list')
        elif isinstance(defn, ast.Union):
            _write_body('union', defn, state)
        elif isinstance(defn, ast.Feature):
            _write_body('feature', defn, state)
        elif isinstance(defn, ast.Const):
            _write_const(defn, state)
        else:
            raise ValueError(
                f'Unexpected node in Mojom definition list: {defn}')

        if i != len(node.definition_list) - 1:
            state.write_line()

    if node.comments_after:
        state.write_line()
        _write_line_comments(node.comments_after, state)


def _write_module(node, state):
    _write_line_comments(node.comments_before, state)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))
    state.write(f'module {node.mojom_namespace};')
    _write_eol(node, state)
    state.write_line()


def _write_import_list(nodes, state):
    no_attributes = []
    with_attributes = []
    for node in nodes:
        if node.attribute_list:
            with_attributes.append(node)
        else:
            no_attributes.append(node)

    sort_key = lambda i: i.import_filename
    no_attributes.sort(key=sort_key)
    with_attributes.sort(key=sort_key)

    for imp in no_attributes:
        _write_import(imp, state)

    if with_attributes:
        state.write_line()
        for i, imp in enumerate(with_attributes):
            _write_import(imp, state)
            if i != len(with_attributes) - 1:
                # Blank line between attributed imports.
                state.write_line()

    if no_attributes or with_attributes:
        # Extra blank line after all imports.
        state.write_line()


def _write_import(node, state):
    assert isinstance(node, ast.Import)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))
    state.write(f'import "{node.import_filename}";')
    _write_eol(node, state)


def _format_attribute_list(node, indent, newline=True):
    """Formats an attribute list.
    Params:
        node: ast.AttributeList or None, The node to format.
        indent: int, The number of spaces to indent for the current scope.
        newline: Whether to insert a `\n` after the attribute list.
    Returns: str, The formatted attribute list.
    """
    if not node:
        return ''
    assert isinstance(node, ast.AttributeList)
    lw = LineWrapper(base_indent=indent)
    lw.write('[')
    for i, attr in enumerate(node):
        lw.write(attr.key.name)
        if not (isinstance(attr.value, bool) and attr.value):
            lw.write('=')
            if isinstance(attr.value, (ast.Name, ast.Identifier, int)):
                lw.write(str(attr.value))
            elif isinstance(attr.value, str):
                lw.write('"')
                lw.write(attr.value.replace('"', '\\"'))
                lw.write('"')
            else:
                raise ValueError(
                    f'Unxpected value type {type(attr.value)} for {attr}')
        if i != len(node.items) - 1:
            lw.write(', ')
    lw.write(']')
    if newline:
        lw.write('\n')
    return lw.finish()


def _write_eol(node, state):
    """Finishes a line after formatting a node. Emits suffix comments and
    the `\n`.
    """
    if node.comments_suffix:
        assert len(node.comments_suffix) == 1
        state.write(f'  {node.comments_suffix[0].value}')
    state.write_line()


def _write_line_comments(comments, state, indent=None):
    """Writes line/block comments.
    Params:
        comments: list of LexToken or None, The comments to format.
        state: A FormatState.
        indent: int, The number of spaces for the base indent of the scope.
    """
    if not comments:
        return
    last_line_no = None
    for comment in comments:
        if last_line_no is not None and comment.lineno > last_line_no + 1:
            # Preserve a single newline of significant whitespace between
            # comments.
            state.write_line()
        _write_comment(comment, state, indent)
        last_line_no = comment.lineno + comment.value.count('\n')
        if comment.lineno == 1:
            # Always a blank line after the file copyright.
            state.write_line()
            last_line_no += 1


def _write_comment(comment, state, indent=None):
    """Writes an individual comment. See _write_line_comments()."""
    assert isinstance(comment, parser.lex.LexToken)
    lines = comment.value.split('\n')
    indent = indent or state.get_indent()
    leader = ' ' * indent
    for line in lines:
        line = line.strip()
        if indent + len(line) <= LINE_LENGTH:
            state.write_line(f'{leader}{line}')
            continue
        if line.startswith('//'):
            line = line[2:].lstrip()
        if not line:
            state.write_line(f'{leader}//')
            continue
        while len(line) > 0:
            state.write(f'{leader}// ')

            # The line fits within the limit.
            if len(line) + state.col <= LINE_LENGTH:
                state.write_line(line)
                break

            # Find a place to break the comment and wrap. The +2 is for
            # offsetting `col` by 1 and being <= `LINE_LENGTH`.
            subline = line[:LINE_LENGTH - state.col + 2]
            last_space = subline.rfind(' ')
            if last_space == -1:
                # No such break exists, so just let the line continue.
                state.write_line(line)
                break
            state.write_line(line[:last_space])
            line = line[last_space + 1:]


def _write_body(keyword, node, state, bodyattr='body'):
    """Writes and formats `node` that is known by `keyword` and has an
    ast.NodeListBase body, stored in the `bodyattr` instance variable.
    """
    _write_line_comments(node.comments_before, state)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))

    lw = LineWrapper(base_indent=state.get_indent())
    lw.write(f'{keyword} {node.mojom_name}')
    state.write(lw.finish())

    body = getattr(node, bodyattr)
    if body is None:
        # Typically a [Native] definition.
        state.write_line(';')
        return
    assert isinstance(body, ast.NodeListBase)

    state.write(' {')

    if len(body.items) != 0:
        _write_eol(node, state)
    state.push_scope(node)

    for i, defn in enumerate(body):
        # Preserve significant blank lines between declarations. Force the
        # insertion of one between methods.
        if i > 0:
            start_line = defn.start.line
            if defn.comments_before:
                start_line = defn.comments_before[0].lineno
            if start_line > body.items[i - 1].end.line + 1 or isinstance(
                    body, ast.InterfaceBody):
                state.write_line()

        if isinstance(defn, ast.Const):
            _write_const(defn, state)
        elif isinstance(defn, ast.Enum):
            _write_body('enum', defn, state, bodyattr='enum_value_list')
        elif isinstance(defn, ast.EnumValue):
            _write_enum_value(defn, state)
        elif isinstance(defn, ast.Method):
            _write_method(defn, state)
        elif isinstance(defn, (ast.StructField, ast.UnionField)):
            _write_field(defn, state)
        else:
            raise ValueError(f'Unexpected node in struct body: {defn}')

    if node.comments_after:
        state.write_line()
        _write_line_comments(node.comments_after, state)

    state.pop_scope()
    state.write(' ' * state.get_indent())
    state.write_line('};')


def _write_const(node, state):
    assert isinstance(node, ast.Const)
    _write_line_comments(node.comments_before, state)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))

    lw = LineWrapper(base_indent=state.get_indent())
    lw.write('const ')
    _write_typename(node.typename, lw)
    lw.write(f' {node.mojom_name} = {node.value};')
    state.write(lw.finish())
    _write_eol(node, state)


def _write_enum_value(node, state):
    assert isinstance(node, ast.EnumValue)
    _write_line_comments(node.comments_before, state)
    lw = LineWrapper(base_indent=state.get_indent())
    if attr := _format_attribute_list(node.attribute_list, 0, newline=False):
        lw.write(attr)
        lw.write(' ')
    lw.write(node.mojom_name.name)
    if node.value:
        lw.write(' = ')
        lw.write(str(node.value))
    lw.write(',')
    state.write(lw.finish())
    _write_eol(node, state)


def _write_field(node, state):
    assert isinstance(node, (ast.StructField, ast.UnionField))
    _write_line_comments(node.comments_before, state)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))

    lw = LineWrapper(base_indent=state.get_indent())
    _write_typename(node.typename, lw)
    lw.write(' ')
    lw.write(node.mojom_name.name)
    if node.ordinal:
        lw.write(f'@{node.ordinal.value}')
    if isinstance(node, ast.StructField) and node.default_value:
        lw.write(f' = {node.default_value}')
    lw.write(';')
    state.write(lw.finish())
    _write_eol(node, state)


def _write_method(node, state):
    assert isinstance(node, ast.Method)
    _write_line_comments(node.comments_before, state)
    state.write(_format_attribute_list(node.attribute_list, state.get_indent()))

    lw = LineWrapper(base_indent=state.get_indent())
    lw.write(node.mojom_name.name)
    if node.ordinal:
        lw.write(f'@{node.ordinal.value}')
    lw.write('(')
    state.write(lw.finish())

    _write_parameter_list(node.parameter_list, state, '')
    if node.response_parameter_list:
        _write_parameter_list(node.response_parameter_list, state, '=> (')
    state.write(';')
    _write_eol(node, state)


def _write_parameter_list(param_list, state, prelude):
    """Writes an ast.ParameterList. `prelude` is an optional string to write
    before the parameters (e.g. "=> ("). There are four ways this can format the
    parameters:
        1. All params on the same line that `state` is at upon function entry.
        2. Params on a single continuation line.
        3. Params on individual lines aligned with column that `state` is at
           upon function entry.
        4. Params on individual continuation lines.
    """
    params = [_format_parameter(param) for param in param_list]
    oneline_len = sum([len(p) for p in params]) + 2 * len(params)
    args_have_comments = any(
        p.comments_before or p.comments_suffix for p in param_list)

    already_indented = False
    if not args_have_comments:
        # Cases 1-3 only work if parameters don't have before or suffix
        # comments.
        if (oneline_len + state.col + len(prelude) + 1) < LINE_LENGTH:
            if prelude:
                state.write(' ')
                state.write(prelude)
            state.write(', '.join(params))
            state.write(')')
            return
        if (oneline_len + state.get_indent() + len(prelude) +
                CONTINUATION_SHIFT + 1) < LINE_LENGTH:
            state.write_line()
            state.write(' ' * (state.get_indent() + CONTINUATION_SHIFT))
            state.write(prelude)
            state.write(', '.join(params))
            state.write(')')
            return
        if all(state.col + len(p) + len(prelude) + 1 < LINE_LENGTH
               for p in params):
            indent = state.col - 1
            already_indented = True
            # Continue to writing each on a separate line.

    if not already_indented:
        state.write_line()
        indent = state.get_indent() + CONTINUATION_SHIFT

    if prelude:
        if already_indented:
            state.write(' ')
        else:
            state.write(' ' * (state.get_indent() + CONTINUATION_SHIFT))
        state.write(prelude)
        indent = state.col - 1
        already_indented = True

    for i, param in enumerate(params):
        param_node = param_list.items[i]
        _write_line_comments(param_node.comments_before, state, indent=indent)
        lw = LineWrapper(
            base_indent=indent,
            already_indented=already_indented if i == 0 else False)
        lw.write(param)
        state.write(lw.finish())
        if i != len(params) - 1:
            state.write(',')
            _write_eol(param_node, state)
        elif param_node.comments_suffix:
            _write_eol(param_node, state)
            state.write(' ' * (indent - CONTINUATION_SHIFT))

    state.write(')')


def _format_parameter(node):
    assert isinstance(node, ast.Parameter)
    w = StringIO()
    w.write(_format_attribute_list(node.attribute_list, 0, newline=False))
    if node.attribute_list:
        w.write(' ')
    _write_typename(node.typename, w)
    w.write(f' {node.mojom_name}')
    if node.ordinal:
        w.write(f'@{node.ordinal.value}')
    return w.getvalue()


def _write_typename(tnode, state):
    assert isinstance(tnode, ast.Typename)
    node = tnode.identifier
    if isinstance(node, ast.Array):
        state.write('array<')
        _write_typename(node.value_type, state)
        if node.fixed_size:
            state.write(', ')
            state.write(str(node.fixed_size))
        state.write('>')
    elif isinstance(node, ast.Map):
        state.write('map<')
        state.write(node.key_type.id)
        state.write(', ')
        _write_typename(node.value_type, state)
        state.write('>')
    elif isinstance(node, (ast.Remote, ast.Receiver)):
        state.write('pending_')
        if node.associated:
            state.write('associated_')
        if isinstance(node, ast.Remote):
            state.write('remote')
        else:
            state.write('receiver')
        state.write('<')
        state.write(node.interface.id)
        state.write('>')
    else:
        state.write(node.id)

    if tnode.nullable:
        state.write('?')


class FormatState:
    """FormatState is a structure to hold the output buffer. It keeps track of
    the current scope for indention and tracks the current column position.
    """

    def __init__(self, output):
        self._output = output
        self._scopes = []

        # Column on the current line.
        self.col = 0

    def write(self, s):
        self._output.write(s)
        i = s.rfind('\n')
        if i == -1:
            self.col += len(s)
        else:
            self.col = len(s) - i

    def write_line(self, line=''):
        if line:
            self.write(line)
        self.write('\n')

    def push_scope(self, scope):
        self._scopes.append(scope)

    def pop_scope(self):
        self._scopes.pop()

    def get_indent(self):
        return len(self._scopes) * INDENT_SHIFT


class LineWrapper:
    """A LineWrapper successively writes strings, breaking and wrapping
    on spaces if the line limit is reached and indenting the next line by
    the CONTINUATION_SHIFT.
    """

    def __init__(self, base_indent=0, already_indented=False):
        """Creates a new LineWrapper.

        Params:
            base_indent: int, The number of spaces of the overall indention
                of this scope.
            already_indented: boolean, If the output is already aligned to
                the proper indention. If False, then the `base_indent` is
                emitted first.
        """
        self._line = ''
        self._base_indent = base_indent
        self._already_indented = already_indented

    def write(self, s):
        self._line += s

    def finish(self):
        remaining = self._line
        self._line = None
        out = ''
        col = 0
        indent = ' ' * self._base_indent

        def _append(s):
            nonlocal out, col
            out += s
            col += len(s)

        def _next_line():
            nonlocal out, col, indent
            indent += ' ' * CONTINUATION_SHIFT
            out += '\n'
            col = 0
            _append(indent)

        if self._already_indented:
            col = self._base_indent
        else:
            _append(' ' * self._base_indent)

        while (ls := len(remaining)) > 0:
            # The entire string fits on the current line.
            if ls + col <= LINE_LENGTH:
                _append(remaining)
                break

            i = remaining[:LINE_LENGTH - col].rfind(' ')
            if i == -1:
                # The fragment has no break point within LINE_LENGTH, but see if
                # there is any break point at all.
                i = remaining.find(' ')
            if i == -1:
                # There is no place to break the current line, so let it extend
                # past the LINE_LENGTH. Break immediately and write more.
                while len(out) > 0 and out[-1] == ' ':
                    # Erase any trailing whitespace.
                    out = out[:-1]
                _next_line()
                _append(remaining)
                break

            _append(remaining[:i])
            _next_line()
            remaining = remaining[i + 1:]
        return out


if __name__ == '__main__':
    if len(sys.argv) == 2:
        print(mojom_format(sys.argv[1]))

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
The code generator generates code based on a graph of code fragments.  Each node
of the graph is represented with CodeNode and its subclasses.  This module
provides a collection of the classes that represent code nodes independent from
specific bindings, such as ECMAScript bindings.
"""

from .codegen_accumulator import CodeGenAccumulator
from .codegen_format import format_template
from .codegen_tracing import CodeGenTracing
from .mako_renderer import MakoRenderer
from .mako_renderer import MakoTemplate


def render_code_node(code_node):
    """
    Renders |code_node| and turns it into text letting |code_node| apply all
    necessary changes (side effects).  Returns the resulting text.
    """
    assert isinstance(code_node, CodeNode)
    assert code_node.outer is None

    renderer = code_node.renderer
    accumulator = code_node.accumulator

    accumulated_size = accumulator.total_size()
    while True:
        prev_accumulated_size = accumulated_size
        renderer.reset()
        code_node.render(renderer)
        accumulated_size = accumulator.total_size()
        if (renderer.is_rendering_complete()
                and accumulated_size == prev_accumulated_size):
            break

    return renderer.to_text()


class Likeliness(object):
    """
    Represents how much likely a code node will be executed.

    Used in SymbolScopeNode in order to determine where SymbolDefinitionNodes
    should be inserted.  Likeliness level can change only at SymbolScopeNode.

    Relational operators are supported, and it's guaranteed to be:
      NEVER < UNLIKELY < LIKELY < ALWAYS
    """

    class Level(int):
        pass

    NEVER = Level(0)
    UNLIKELY = Level(1)
    LIKELY = Level(2)
    ALWAYS = Level(3)


class CodeNode(object):
    """
    This is the base class of all code fragment nodes.  CodeNode represents
    a tree of templated text.

    - Tree structure
    CodeNode can be nested and |outer| points to the nesting CodeNode.  Also
    CodeNode can make a sequence and |prev| points to the previous CodeNode.
    See also |ListNode|.

    - Template rendering
    CodeNode has template text and template variable bindings.  Either of
    |__str__| or |render| produces a text of generated code, which is
    accumulated into the |renderer| object.

    - Code generation at rendering time
    It's allowed by design that |__str__| and |render| have side effects on
    rendering states, hence repeated calls may produce different texts.
    However, the resulting text must converge when the rendering is done enough
    times.
    See also |SymbolScopeNode| and |SymbolNode|.
    """

    class _RenderState(object):
        """
        Represents a set of per-render states.  Every call to CodeNode.render
        resets all the per-render states.
        """

        def __init__(self):
            # List of SymbolNodes that are defined at this point of rendering.
            # Used to determine whether a certain symbol is already defined by
            # this point of rendering.
            self.defined_code_symbols = []

            # List of SymbolNodes that are not yet defined at this point of
            # rendering.  SymbolNodes are accumulated in order of their first
            # appearance.  The order affects the insertion order of
            # SymbolDefinitionNodes.
            self.undefined_code_symbols = []

            # Dict from a SymbolNode to a set of tuples of SymbolScopeNodes
            # where the symbol was used.
            #
            # For example, given a code symbol |x|, the following code
            # structure:
            #   {  // Scope1
            #     {  // Scope2A
            #       {  // Scope3
            #         x;          // [1]
            #       }
            #       x;            // [2]
            #     }
            #     x;              // [3]
            #     {  // Scope2B
            #       x;            // [4]
            #     }
            #     x;              // [5]
            #   }
            # is translated into an entry of the dict below:
            #   set([
            #     (Scope1),                   # [3], [5]
            #     (Scope1, Scope2A),          # [2]
            #     (Scope1, Scope2A, Scope3),  # [1]
            #     (Scope1, Scope2B),          # [4]
            #   ])
            self.symbol_to_scope_chains = {}

    _gensym_seq_id = 0

    @classmethod
    def gensym(cls):
        """
        Creates a new template variable that never conflicts with anything.

        The name 'gensym' came from 'gensym' (generated symbol) in Lisp that
        exists for exactly the same purpose.

        Note that |gensym| is used to produce a new Mako template variable while
        SymbolNode is used to represent a code symbol (such as a local variable)
        in generated code.

        Bad example:
            template_text = "abc ${tmp} xyz"
            a = CodeNodeA(template_text='123')
            b = CodeNodeB(template_text=template_text, {'tmp': a})
        |b| expects "abc 123 xyz" but what if 'tmp' were already bound to
        something else?

        Good example:
            sym = CodeNode.gensym()
            template_text = format_template(
                "abc ${{{node_a}}} xyz", node_a=sym)
            a = CodeNodeA(template_text='123')
            b = CodeNodeB(template_text=template_text, {sym: a})
        "{{" and "}}" are literal of "{" and "}" themselves, and the innermost
        "{node_a}" will be replaced with |sym|.  The resulting template text
        will be "abc ${gensym1} xyz" when |sym| is 'gensym1'.
        """
        cls._gensym_seq_id += 1
        return "gensym{}".format(cls._gensym_seq_id)

    def __init__(self, template_text=None, template_vars=None):
        assert template_text is None or isinstance(template_text, str)
        assert template_vars is None or isinstance(template_vars, dict)

        # The outer CodeNode or None iff this is a top-level node
        self._outer = None
        # The previous CodeNode if this is a Sequence or None
        self._prev = None

        # Mako's template text, bindings dict
        if template_text is None:
            self._template = None
        else:
            self._template = MakoTemplate(template_text)

        # Template variable bindings
        self._own_template_vars = None
        self._base_template_vars = None
        self._cached_template_vars = None

        self._accumulator = None  # CodeGenAccumulator
        self._accumulate_requests = None

        self._renderer = None  # MakoRenderer

        self._render_state = CodeNode._RenderState()
        self._is_rendering = False

        if template_vars:
            self.add_template_vars(template_vars)

        # For ease of debugging, render which line of Python code created this
        # CodeNode object when the code generation tracing is enabled.
        self._cg_tracing_caller_callframe = None

    def __str__(self):
        """
        Renders this CodeNode object directly into the renderer's text buffer
        and always returns the empty string.  This is because it's faster to
        accumulate the rendering result directly in a single text buffer than
        making a lot of string pieces and concatenating them.

        This function is supposed to be used in a Mako template as ${code_node}.
        """
        renderer = self.renderer
        assert renderer

        self.render(renderer)
        return ""

    def render(self, renderer):
        """
        Renders this CodeNode object as a text string and also propagates
        updates to related CodeNode objects.  As this method has side-effects
        not only to this object but also other related objects, the resulting
        text may change on each invocation.
        """
        last_render_state = self._render_state
        self._render_state = CodeNode._RenderState()
        self._is_rendering = True

        try:
            self._render(
                renderer=renderer, last_render_state=last_render_state)
            if CodeGenTracing.is_enabled():
                if self._cg_tracing_caller_callframe:
                    renderer.render_text(str(
                        self._cg_tracing_caller_callframe))
        finally:
            self._is_rendering = False

        if self._accumulate_requests:
            accumulator = self.accumulator
            assert accumulator
            for request in self._accumulate_requests:
                request(accumulator)
            self._accumulate_requests = None

    def _render(self, renderer, last_render_state):
        """
        Renders this CodeNode object as a text string and also propagates
        updates to related CodeNode objects.

        Only limited subclasses may override this method.
        """
        renderer.render(
            caller=self,
            template=self._template,
            template_vars=self.template_vars)

    @property
    def outer(self):
        """Returns the outer CodeNode or None iff this is a top-level node."""
        return self._outer

    def set_outer(self, outer):
        assert isinstance(outer, CodeNode)
        assert self._outer is None
        self._outer = outer

    def reset_outer(self, outer):
        assert isinstance(outer, CodeNode) or outer is None
        self._outer = outer

    @property
    def prev(self):
        """Returns the previous CodeNode if this is a Sequence or None."""
        return self._prev

    def set_prev(self, prev):
        assert isinstance(prev, CodeNode)
        assert self._prev is None
        self._prev = prev

    def reset_prev(self, prev):
        assert isinstance(prev, CodeNode) or prev is None
        self._prev = prev

    def outer_scope(self):
        """Returns the outer scope closest to this scope or None."""
        node = self.outer
        while node is not None:
            if isinstance(node, SymbolScopeNode):
                return node
            node = node.outer
        return None

    def outermost(self):
        """Returns the outermost node, i.e. the node whose |outer| is None."""
        node = self
        while node.outer is not None:
            node = node.outer
        return node

    def inclusive_outers(self):
        """
        Returns a list of outer nodes including this node in order from this
        node to the outermost node.
        """
        outers = []
        node = self
        while node is not None:
            outers.append(node)
            node = node.outer
        return outers

    @property
    def template_vars(self):
        """
        Returns the template variable bindings available at this point, i.e.
        bound at this node or outer nodes.

        CAUTION: This accessor caches the result.  This accessor must not be
        called during construction of a code node tree.
        """
        if self._cached_template_vars is not None:
            return self._cached_template_vars

        outers = self.inclusive_outers()
        bindings = None

        for node in outers:
            if node.base_template_vars is not None:
                bindings = dict(node.base_template_vars)
                break
        if bindings is None:
            bindings = {}

        for node in outers:
            if node.own_template_vars is None:
                continue
            for name, value in node.own_template_vars.items():
                assert name not in bindings, (
                    "Duplicated template variable binding: {}".format(name))
                bindings[name] = value

        self._cached_template_vars = bindings
        return self._cached_template_vars

    @property
    def own_template_vars(self):
        """Returns the template variables bound at this code node."""
        return self._own_template_vars

    def add_template_var(self, name, value):
        if self._own_template_vars is None:
            self._own_template_vars = {}
        assert isinstance(name, str)
        assert name not in self._own_template_vars, (
            "Duplicated template variable binding: {}".format(name))
        if isinstance(value, CodeNode):
            value.set_outer(self)
        self._own_template_vars[name] = value

    def add_template_vars(self, template_vars):
        assert isinstance(template_vars, dict)
        for name, value in template_vars.items():
            self.add_template_var(name, value)

    @property
    def base_template_vars(self):
        """
        Returns the base template variables if it's set at this code node.

        The base template variables are a set of template variables that of
        the innermost code node takes effect.  It means that the base template
        variables are layered and shadowable.
        """
        return self._base_template_vars

    def set_base_template_vars(self, template_vars):
        assert isinstance(template_vars, dict)
        for name, value in template_vars.items():
            assert isinstance(name, str)
            assert not isinstance(value, CodeNode)
        assert self._base_template_vars is None
        self._base_template_vars = template_vars

    @property
    def accumulator(self):
        # Always consistently use the accumulator of the root node.
        if self.outer is None:
            return self._accumulator
        return self.outermost().accumulator

    def set_accumulator(self, accumulator):
        assert isinstance(accumulator, CodeGenAccumulator)
        assert self._accumulator is None
        self._accumulator = accumulator

    def accumulate(self, request):
        """
        While rendering the code node, |request| will be called with the
        argument of self.accumulator.
        """
        assert callable(request)
        if self._accumulate_requests is None:
            self._accumulate_requests = []
        self._accumulate_requests.append(request)

    @property
    def renderer(self):
        # Always consistently use the renderer of the root node.
        if self.outer is None:
            return self._renderer
        return self.outermost().renderer

    def set_renderer(self, renderer):
        assert isinstance(renderer, MakoRenderer)
        assert self._renderer is None
        self._renderer = renderer

    @property
    def current_render_state(self):
        assert self._is_rendering
        return self._render_state

    @property
    def last_render_state(self):
        assert not self._is_rendering
        return self._render_state

    def on_code_symbol_referenced(self, symbol_node, symbol_scope_chain):
        """Receives a report of use of a symbol node."""
        assert isinstance(symbol_node, SymbolNode)
        assert isinstance(symbol_scope_chain, tuple)
        assert all(
            isinstance(scope, SymbolScopeNode) for scope in symbol_scope_chain)
        self.current_render_state.symbol_to_scope_chains.setdefault(
            symbol_node, set()).add(symbol_scope_chain)

    def capture_caller_for_tracing(self):
        """Captures the caller function information for a debugging purpose."""
        assert not self._cg_tracing_caller_callframe
        self._cg_tracing_caller_callframe = CodeGenTracing.capture_caller()


class EmptyNode(CodeNode):
    """Represents the zero-length text and renders nothing."""

    def __init__(self):
        CodeNode.__init__(self)

    def _render(self, renderer, last_render_state):
        pass


class LiteralNode(CodeNode):
    """
    Represents a literal text, which will be rendered as is without any template
    magic applied.  The given literal text object will be stringified on each
    rendering.
    """

    def __init__(self, literal_text):
        CodeNode.__init__(self)

        self._literal_text = literal_text

    def _render(self, renderer, last_render_state):
        renderer.push_caller(self)
        try:
            renderer.render_text(str(self._literal_text))
        finally:
            renderer.pop_caller()


def TextNode(template_text):
    """
    Represents a template text node.

    TextNode is designed to be a leaf node of a code node tree.  TextNode
    represents a template text while LiteralNode represents a literal text.
    All template magics will be applied to |template_text|.

    This function is pretending to be a CodeNode subclass and instantiates one
    of text-ish code node subclass depending on the content of |template_text|.
    """
    assert isinstance(template_text, str)

    if "$" in template_text or "%" in template_text:
        node = _TextNode(template_text)
        if CodeGenTracing.is_enabled():
            node.capture_caller_for_tracing()
        return node
    elif template_text:
        node = LiteralNode(template_text)
        if CodeGenTracing.is_enabled():
            node.capture_caller_for_tracing()
        return node
    else:
        return EmptyNode()


class _TextNode(CodeNode):
    """
    Represents a template text node.

    TextNode is designed to be a leaf node of a code node tree.  TextNode
    represents a template text while LiteralNode represents a literal text.
    All template magics will be applied to |template_text|.
    """

    def __init__(self, template_text):
        CodeNode.__init__(self, template_text=template_text)


def FormatNode(format_string, *args, **argv):
    """
    Represents a template text node, which is produced by applying
    codegen_format.format_template to the arguments.
    """
    return TextNode(format_template(format_string, *args, **argv))


class CompositeNode(CodeNode):
    """
    Represents a composition of multiple code nodes.  Composition will be done
    by using |CodeNode.gensym| so that it won't contaminate a namespace of the
    template variables.
    """

    def __init__(self, template_format_str, *args, **kwargs):
        """
        Args:
            template_format_str: A format string that is used to produce the
                template text.
            args:
            kwargs: Arguments to be passed to |format_template|.  Not
                necessarily be CodeNode, but also anything renderable can be
                passed in.
        """
        assert isinstance(template_format_str, str)
        gensym_args = []
        gensym_kwargs = {}
        template_vars = {}
        for arg in args:
            assert isinstance(arg, (CodeNode, int, str))
            gensym = CodeNode.gensym()
            gensym_args.append("${{{}}}".format(gensym))
            template_vars[gensym] = arg
        for key, value in kwargs.items():
            assert isinstance(key, (int, str))
            assert isinstance(value, (CodeNode, int, str))
            gensym = CodeNode.gensym()
            gensym_kwargs[key] = "${{{}}}".format(gensym)
            template_vars[gensym] = value
        template_text = format_template(template_format_str, *gensym_args,
                                        **gensym_kwargs)

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)


class ListNode(CodeNode):
    """
    Represents a list of nodes.

    append, extend, insert, and remove work just like built-in list's methods
    except that addition and removal of None have no effect.
    """

    def __init__(self, code_nodes=None, separator="\n", head="", tail=""):
        """
        Args:
            code_nodes: A list of CodeNode to be rendered.
            separator: A str inserted between code nodes.
            head:
            tail: The head and tail sections that will be rendered iff the
                content list is not empty.
        """
        assert isinstance(separator, str)
        assert isinstance(head, str)
        assert isinstance(tail, str)

        CodeNode.__init__(self)

        self._element_nodes = []
        self._separator = separator
        self._head = head
        self._tail = tail

        self._will_skip_separator = False

        if code_nodes is not None:
            self.extend(code_nodes)

    def __getitem__(self, index):
        return self._element_nodes[index]

    def __iter__(self):
        return iter(self._element_nodes)

    def __len__(self):
        return len(self._element_nodes)

    def _render(self, renderer, last_render_state):
        renderer.push_caller(self)
        try:
            if self._element_nodes:
                renderer.render_text(self._head)
            self._will_skip_separator = True
            for node in self._element_nodes:
                if self._will_skip_separator:
                    self._will_skip_separator = False
                else:
                    renderer.render_text(self._separator)
                node.render(renderer)
            if self._element_nodes:
                renderer.render_text(self._tail)
        finally:
            renderer.pop_caller()

    def skip_separator(self):
        self._will_skip_separator = True

    def append(self, node):
        if node is None:
            return
        assert isinstance(node, CodeNode)
        assert node.outer is None and node.prev is None

        if len(self._element_nodes) == 0:
            self._element_nodes.append(node)
        else:
            node.set_prev(self._element_nodes[-1])
            self._element_nodes.append(node)
        node.set_outer(self)

    def extend(self, nodes):
        for node in nodes:
            self.append(node)

    def insert(self, index, node):
        if node is None:
            return
        assert isinstance(index, int)
        assert isinstance(node, CodeNode)
        assert node.outer is None and node.prev is None

        if index < 0:
            index += len(self._element_nodes)
        index = max(0, min(index, len(self._element_nodes)))

        if (len(self._element_nodes) == 0
                or index == len(self._element_nodes)):
            return self.append(node)

        next_node = self._element_nodes[index]
        if next_node.prev:
            node.set_prev(next_node.prev)
        next_node.reset_prev(node)
        node.set_outer(self)
        self._element_nodes.insert(index, node)

    def remove(self, node):
        if node is None:
            return
        assert node in self

        index = self._element_nodes.index(node)
        if index + 1 < len(self._element_nodes):
            next_node = self._element_nodes[index + 1]
            prev_node = self._element_nodes[index - 1] if index != 0 else None
            next_node.reset_prev(prev_node)
        del self._element_nodes[index]
        node.reset_outer(None)
        node.reset_prev(None)


class SequenceNode(ListNode):
    """
    Represents a sequence of generated code without introducing any new scope,
    and provides the points where SymbolDefinitionNodes can be inserted.
    """

    def __init__(self, code_nodes=None, separator="\n", head="", tail=""):
        ListNode.__init__(
            self,
            code_nodes=code_nodes,
            separator=separator,
            head=head,
            tail=tail)

        self._to_be_removed = []

    def _render(self, renderer, last_render_state):
        if self._to_be_removed:
            for node in self._to_be_removed:
                self.remove(node)
            self._to_be_removed = []

        super(SequenceNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)

    def schedule_to_remove(self, node):
        """Schedules a task to remove the |node| in the next rendering cycle."""
        assert node in self
        self._to_be_removed.append(node)


class SymbolScopeNode(SequenceNode):
    """
    Represents a scope of generated code.

    If SymbolNodes are rendered inside this node, this node will attempt to
    insert corresponding SymbolDefinitionNodes appropriately.
    """

    def __init__(self, code_nodes=None, separator="\n", head="", tail=""):
        SequenceNode.__init__(
            self,
            code_nodes=code_nodes,
            separator=separator,
            head=head,
            tail=tail)

        self._likeliness = Likeliness.ALWAYS
        self._registered_code_symbols = set()
        self._referenced_code_symbols = set()

    def _render(self, renderer, last_render_state):
        for symbol_node in last_render_state.undefined_code_symbols:
            assert self.is_code_symbol_registered(symbol_node)
            self._referenced_code_symbols.add(symbol_node)
            if not self.is_code_symbol_defined(symbol_node):
                self._insert_symbol_definition(symbol_node, last_render_state)

        super(SymbolScopeNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)

        if self.current_render_state.undefined_code_symbols:
            renderer.invalidate_rendering_result()

    def _insert_symbol_definition(self, symbol_node, last_render_state):
        DIRECT_USES = "u"
        DIRECT_CHILD_SCOPES = "s"
        ANALYSIS_RESULT_KEYS = (
            # Number of direct uses in this scope
            DIRECT_USES,
            # Number of direct child scopes
            DIRECT_CHILD_SCOPES,
            # Number of direct child scopes per likeliness
            Likeliness.ALWAYS,
            Likeliness.LIKELY,
            Likeliness.UNLIKELY,
        )

        def analyze_symbol_usage(render_state):
            counts = dict.fromkeys(ANALYSIS_RESULT_KEYS, 0)

            scope_chains = render_state.symbol_to_scope_chains.get(symbol_node)
            if not scope_chains:
                return counts

            self_index = next(iter(scope_chains)).index(self)
            scope_chains = map(
                lambda scope_chain: scope_chain[self_index + 1:], scope_chains)
            scope_to_likeliness = {}
            for scope_chain in scope_chains:
                if not scope_chain:
                    counts[DIRECT_USES] += 1
                else:
                    likeliness = min(
                        map(lambda scope: scope.likeliness, scope_chain))
                    scope = scope_chain[0]
                    scope_to_likeliness[scope] = max(
                        likeliness, scope_to_likeliness.get(scope, likeliness))
            for likeliness in scope_to_likeliness.values():
                counts[DIRECT_CHILD_SCOPES] += 1
                counts[likeliness] += 1
            return counts

        def likeliness_at(render_state):
            counts = analyze_symbol_usage(render_state)
            if counts[DIRECT_USES] >= 1:
                return Likeliness.ALWAYS
            for likeliness in (Likeliness.ALWAYS, Likeliness.LIKELY,
                               Likeliness.UNLIKELY):
                if counts[likeliness] > 0:
                    return likeliness
            return Likeliness.NEVER

        def insert_before_threshold(sequence_node, threshold):
            for index, node in enumerate(sequence_node):
                if (isinstance(node, SequenceNode)
                        and not isinstance(node, SymbolScopeNode)):
                    did_insert = insert_before_threshold(node, threshold)
                    if did_insert:
                        return True
                elif likeliness_at(node.last_render_state) >= threshold:
                    sequence_node.insert(index,
                                         symbol_node.create_definition_node())
                    return True
            return False

        counts = analyze_symbol_usage(last_render_state)
        if counts[DIRECT_USES] >= 1:
            did_insert = insert_before_threshold(self, Likeliness.UNLIKELY)
            assert did_insert
        elif counts[DIRECT_CHILD_SCOPES] == 1:
            pass  # Let the child SymbolScopeNode do the work.
        elif counts[Likeliness.ALWAYS] + counts[Likeliness.LIKELY] >= 2:
            did_insert = insert_before_threshold(self, Likeliness.LIKELY)
            assert did_insert
        else:
            pass  # Let descendant SymbolScopeNodes do the work.

    def is_code_symbol_registered(self, symbol_node):
        """
        Returns True if |symbol_node| is registered and available for use within
        this scope.
        """
        assert isinstance(symbol_node, SymbolNode)

        if symbol_node in self._registered_code_symbols:
            return True

        outer = self.outer_scope()
        if outer is None:
            return False
        return outer.is_code_symbol_registered(symbol_node)

    def find_code_symbol(self, name):
        """
        Returns a SymbolNode whose name is the given |name| and which is
        available for use within this scope, or None if not found.
        """
        assert isinstance(name, str)

        for symbol_node in self._registered_code_symbols:
            if symbol_node.name == name:
                return symbol_node

        outer = self.outer_scope()
        if outer is None:
            return None
        return outer.find_code_symbol(name)

    def register_code_symbol(self, symbol_node):
        """Registers a SymbolNode and makes it available in this scope."""
        assert isinstance(symbol_node, SymbolNode)
        self.add_template_var(symbol_node.name, symbol_node)
        self._registered_code_symbols.add(symbol_node)

    def register_code_symbols(self, symbol_nodes):
        for symbol_node in symbol_nodes:
            self.register_code_symbol(symbol_node)

    @property
    def referenced_code_symbols(self):
        """Returns SymbolNodes that have once been referenced in this scope."""
        return frozenset(self._referenced_code_symbols)

    @property
    def likeliness(self):
        """
        Returns how much likely that this SymbolScopeNode will be executed in
        runtime.  The likeliness is relative to the closest outer
        SymbolScopeNode.
        """
        return self._likeliness

    def set_likeliness(self, likeliness):
        assert isinstance(likeliness, Likeliness.Level)
        self._likeliness = likeliness

    def is_code_symbol_defined(self, symbol_node):
        """
        Returns True if |symbol_node| is defined in this scope by the moment
        when the method is called.
        """
        assert isinstance(symbol_node, SymbolNode)

        if symbol_node in self.current_render_state.defined_code_symbols:
            return True

        outer = self.outer_scope()
        if outer is None:
            return False
        return outer.is_code_symbol_defined(symbol_node)

    def on_code_symbol_defined(self, symbol_node):
        """Receives a report that a symbol gets defined."""
        assert isinstance(symbol_node, SymbolNode)
        self.current_render_state.defined_code_symbols.append(symbol_node)

    def on_undefined_code_symbol_found(self, symbol_node):
        """Receives a report of use of an undefined symbol node."""
        assert isinstance(symbol_node, SymbolNode)
        state = self.current_render_state
        if symbol_node not in state.undefined_code_symbols:
            state.undefined_code_symbols.append(symbol_node)


class SymbolNode(CodeNode):
    """
    Represents a code symbol such as a local variable of generated code.

    Using a SymbolNode combined with SymbolScopeNode, SymbolDefinitionNode(s)
    will be automatically inserted iff this symbol is referenced.
    """

    def __init__(self, name, template_text=None, definition_constructor=None):
        """
        Args:
            name: The name of this code symbol.
            template_text: Template text to be used to define the code symbol.
            definition_constructor: A callable that creates and returns a new
                definition node.  This SymbolNode will be passed as the
                argument.
                Either of |template_text| or |definition_constructor| must be
                given.
        """
        assert isinstance(name, str) and name

        CodeNode.__init__(self)

        self._name = name
        self._cg_tracing_caller_callframe_of_symbol_node = None

        if template_text is not None:
            assert isinstance(template_text, str)
            assert definition_constructor is None

            # A SymbolDefinitionNode will be automatically inserted on demand,
            # and the caller of this `__init__` is the code that actually
            # defines a SymbolDefinitionNode. So, save the caller and pass it
            # to a SymbolDefinitionNode.
            if CodeGenTracing.is_enabled():
                self._cg_tracing_caller_callframe_of_symbol_node = (
                    CodeGenTracing.capture_caller())

            def constructor(symbol_node):
                text_node = TextNode(template_text)
                # Overwrite the caller's callframe with the one saved in the
                # SymbolNode, which is the actual caller's callframe.
                if CodeGenTracing.is_enabled():
                    text_node._cg_tracing_caller_callframe = (
                        symbol_node._cg_tracing_caller_callframe_of_symbol_node
                    )
                return SymbolDefinitionNode(symbol_node=symbol_node,
                                            code_nodes=[text_node])

            self._definition_constructor = constructor
        else:
            assert template_text is None
            assert callable(definition_constructor)

            self._definition_constructor = definition_constructor

    def _render(self, renderer, last_render_state):
        self._request_symbol_definition(renderer)

        renderer.render_text(self.name)

    def request_symbol_definition(self):
        self._request_symbol_definition(self.renderer)

    def _request_symbol_definition(self, renderer):
        symbol_scope_chain = tuple(
            filter(lambda node: isinstance(node, SymbolScopeNode),
                   renderer.callers_from_first_to_last))

        for caller in renderer.callers_from_last_to_first:
            caller.on_code_symbol_referenced(self, symbol_scope_chain)
            if caller is self.outer:
                break

        if not symbol_scope_chain[-1].is_code_symbol_defined(self):
            for scope in reversed(symbol_scope_chain):
                scope.on_undefined_code_symbol_found(self)
                if scope is self.outer:
                    break

    @property
    def name(self):
        return self._name

    def create_definition_node(self):
        """Creates a new definition node."""
        node = self._definition_constructor(self)
        assert isinstance(node, SymbolDefinitionNode)
        assert node.target_symbol is self
        return node


class SymbolDefinitionNode(SequenceNode):
    """
    Represents a definition of a code symbol.

    It's allowed to define the same code symbol multiple times, and most
    upstream definition(s) are effective.
    """

    def __init__(self, symbol_node, code_nodes=None):
        assert isinstance(symbol_node, SymbolNode)

        SequenceNode.__init__(self, code_nodes)

        self._symbol_node = symbol_node

    def _render(self, renderer, last_render_state):
        scope = self.outer_scope()
        if scope.is_code_symbol_defined(self._symbol_node):
            assert isinstance(self.outer, SequenceNode)
            self.outer.schedule_to_remove(self)
            self.outer.skip_separator()
            return

        scope.on_code_symbol_defined(self._symbol_node)

        super(SymbolDefinitionNode, self)._render(
            renderer=renderer, last_render_state=last_render_state)

    @property
    def target_symbol(self):
        return self._symbol_node


class WeakDependencyNode(CodeNode):
    """
    Represents weak dependencies to SymbolNodes, where "weak" means that this
    code node itself does not require any SymbolDefinitionNode of the target
    symbols, however, once any other code node within the closest outer scope
    requires a symbol definition, then this code node also requires the symbol
    definition, too.  This makes the symbol definition node be placed prior to
    this node iff such a definition is added.

    In short, you can control the position of SymbolDefinitionNode with using
    WeakDependencyNode without requiring the symbol definition node.
    """

    def __init__(self, dep_syms):
        """
        Args:
            dep_syms: A list of code symbol names on which this code node
                weakly depends.
        """
        assert isinstance(dep_syms, (list, tuple))
        assert all(isinstance(sym, str) for sym in dep_syms)

        CodeNode.__init__(self)

        # Registered weak dependencies to symbols.
        self._weak_dep_sym_names = tuple(dep_syms)
        # Symbol names that have not yet turned into strong references.
        self._weak_dep_sym_queue = list(self._weak_dep_sym_names)
        # SymbolNodes that already turned into strong references.
        self._strong_dep_symbol_nodes = []

    def _render(self, renderer, last_render_state):
        renderer.push_caller(self)
        try:
            self._render_internal()
        finally:
            renderer.pop_caller()

    def _render_internal(self):
        for symbol_node in self._strong_dep_symbol_nodes:
            symbol_node.request_symbol_definition()

        if not self._weak_dep_sym_queue:
            return

        referenced_code_symbols = self.outer_scope().referenced_code_symbols
        unprocessed_sym_names = []
        for weak_sym_name in self._weak_dep_sym_queue:
            for symbol_node in referenced_code_symbols:
                if symbol_node.name == weak_sym_name:
                    symbol_node.request_symbol_definition()
                    self._strong_dep_symbol_nodes.append(symbol_node)
                    break
            else:
                unprocessed_sym_names.append(weak_sym_name)
        self._weak_dep_sym_queue = unprocessed_sym_names


class SymbolSensitiveSelectionNode(CodeNode):
    """
    Represents a code node sensitive to the symbol definitions available at
    this point of rendering.

    Given multiple choices of code nodes, this code node renders one of them
    according to what code symbols are already defined at this point of
    rendering.

    Example:
        choice1 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=["a", "b"], code_node=code_node1)
        choice2 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=["x"], code_node=code_node2)
        choice3 = SymbolSensitiveSelectionNode.Choice(
            symbol_names=[], code_node=code_node3)
        node = SymbolSensitiveSelectionNode([choice1, choice2, choice3])

    If code symbols "a" and "b" are both already defined, |code_node1| is
    rendered.  Otherwise if "x" is already defined, |code_node2| is rendered.
    Otherwise, |code_node3| is rendered.
    """

    class Choice(object):
        """Represents a choice in SymbolSensitiveSelectionNode."""

        def __init__(self, symbol_names, code_node):
            """
            Args:
                symbol_names: Names of the code symbols to be defined prior to
                    the SymbolSensitiveSelectionNode in order to get selected.
                    All code symbols need to be defined to get selected.  The
                    empty list satisfies the condition, so behaves as the
                    default choice.
                code_node: The code node to be rendered when this Choice gets
                    selected.
            """
            assert isinstance(symbol_names, (list, tuple))
            assert all(isinstance(name, str) for name in symbol_names)
            assert isinstance(code_node, CodeNode)

            self._symbol_names = tuple(symbol_names)
            self._code_node = code_node

        @property
        def symbol_names(self):
            return self._symbol_names

        @property
        def code_node(self):
            return self._code_node

    def __init__(self, choices):
        """
        Args:
            choices: A list of Choices of code nodes, in the order of priority.
        """
        assert isinstance(choices, (list, tuple))
        assert all(isinstance(choice, self.Choice) for choice in choices)

        CodeNode.__init__(self)

        self._choices = tuple(choices)
        for choice in self._choices:
            choice.code_node.set_outer(self)

    def _render(self, renderer, last_render_state):
        renderer.push_caller(self)
        try:
            self._render_internal(renderer)
        finally:
            renderer.pop_caller()

    def _render_internal(self, renderer):
        scope = self.outer_scope()

        for choice in self._choices:
            for name in choice.symbol_names:
                symbol_node = scope.find_code_symbol(name)
                if not (symbol_node
                        and scope.is_code_symbol_defined(symbol_node)):
                    break
            else:
                return choice.code_node.render(renderer)

        # Do not raise an error because it's possible that more
        # SymbolDefinitionNodes will be added in the future rendering
        # iterations and this error will be resolved in the end state despite
        # that there is no guarantee to be resolved.
        renderer.render_text("<<unresolved SymbolSensitiveSelectionNode>>")

# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This module provides C++ language specific implementations of
code_node.CodeNode.
"""

from .code_node import CodeNode
from .code_node import CompositeNode
from .code_node import EmptyNode
from .code_node import Likeliness
from .code_node import ListNode
from .code_node import SymbolScopeNode
from .code_node import TextNode
from .code_node import WeakDependencyNode
from .codegen_expr import CodeGenExpr
from .codegen_format import format_template


class CxxBlockNode(CompositeNode):
    def __init__(self, body):
        template_format = (
            "{{\n"  #
            "  {body}\n"
            "}}")

        CompositeNode.__init__(
            self,
            template_format,
            body=_to_symbol_scope_node(body, Likeliness.ALWAYS))


class CxxIfNode(CompositeNode):

    def __init__(self, cond, attribute, body, likeliness):
        attribute = attribute + ' ' if (attribute is not None) else ''
        template_format = (
            "if ({cond}) {attribute}{{\n"  #
            "  {body}\n"
            "}}")

        CompositeNode.__init__(self,
                               template_format,
                               cond=_to_conditional_node(cond),
                               attribute=_to_maybe_text_node(attribute),
                               body=_to_symbol_scope_node(body, likeliness))


class CxxIfElseNode(CompositeNode):

    def __init__(self, cond, attribute, then, then_likeliness, else_,
                 else_likeliness):
        attribute = attribute + ' ' if (attribute is not None) else ''
        template_format = ("if ({cond}) {attribute}{{\n"
                           "  {then}\n"
                           "}} else {{\n"
                           "  {else_}\n"
                           "}}")

        CompositeNode.__init__(
            self,
            template_format,
            cond=_to_conditional_node(cond),
            attribute=_to_maybe_text_node(attribute),
            then=_to_symbol_scope_node(then, then_likeliness),
            else_=_to_symbol_scope_node(else_, else_likeliness))


class CxxLikelyIfNode(CxxIfNode):

    def __init__(self, cond, attribute, body):
        CxxIfNode.__init__(self, cond, attribute, body, Likeliness.LIKELY)


class CxxUnlikelyIfNode(CxxIfNode):

    def __init__(self, cond, attribute, body):
        CxxIfNode.__init__(self, cond, attribute, body, Likeliness.UNLIKELY)


class CxxMultiBranchesNode(CodeNode):
    class _Clause(object):
        def __init__(self, cond, body):
            assert isinstance(cond, (CodeNode, bool))
            assert isinstance(body, SymbolScopeNode)
            self.cond = cond
            self.body = body

    def __init__(self):
        clauses_gensym = CodeNode.gensym()
        clauses = []
        template_text = format_template(
            """\
% for {clause} in {clauses}:
% if not loop.first:
 else \\
% endif
% if {clause}.cond is not False:
% if {clause}.cond is not True:
if (${{{clause}.cond}}) \\
% endif
{{
  ${{{clause}.body}}
}}\\
% if {clause}.cond is True:
  <% break %>
% endif
% endif
% endfor\
""",
            clause=CodeNode.gensym(),
            clauses=clauses_gensym)
        template_vars = {clauses_gensym: clauses}

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)

        self._clauses = clauses

    def append(self, cond, body, likeliness=Likeliness.LIKELY):
        if cond is None:
            cond = False
        elif isinstance(cond, CodeGenExpr):
            if cond.is_always_true:
                cond = True
            elif cond.is_always_false:
                cond = False

        if not isinstance(cond, bool):
            cond = _to_conditional_node(cond)
        body = _to_symbol_scope_node(body, likeliness)

        if isinstance(cond, CodeNode):
            cond.set_outer(self)
        body.set_outer(self)

        self._clauses.append(self._Clause(cond, body))


class CxxSwitchNode(CodeNode):
    class _Clause(object):
        def __init__(self, case, body, should_add_break):
            assert isinstance(case, CodeNode) or case is None
            assert isinstance(body, SymbolScopeNode)
            assert isinstance(should_add_break, bool)
            self.case = case
            self.body = body
            self.should_add_break = should_add_break

    def __init__(self, cond):
        cond = _to_conditional_node(cond)
        cond_gensym = CodeNode.gensym()
        clauses_gensym = CodeNode.gensym()
        clauses = []
        default_clauses_gensym = CodeNode.gensym()
        default_clauses = []
        template_text = format_template(
            """\
switch (${{{cond}}}) {{
% for {clause} in {clauses}:
  case ${{{clause}.case}}: {{
    ${{{clause}.body}}
% if {clause}.should_add_break:
    break;
% endif
  }}
% endfor
% for {clause} in {default_clauses}:
  default: {{
    ${{{clause}.body}}
% if {clause}.should_add_break:
    break;
% endif
  }}
% endfor
}}\
""",
            cond=cond_gensym,
            clause=CodeNode.gensym(),
            clauses=clauses_gensym,
            default_clauses=default_clauses_gensym)
        template_vars = {
            cond_gensym: cond,
            clauses_gensym: clauses,
            default_clauses_gensym: default_clauses,
        }

        CodeNode.__init__(
            self, template_text=template_text, template_vars=template_vars)

        self._clauses = clauses
        self._default_clauses = default_clauses

    def append(self,
               case,
               body,
               should_add_break=True,
               likeliness=Likeliness.LIKELY):
        """
        Args:
            case: Constant expression of 'case' label, or None for 'default'
                label.
            body: The body statements.
            should_add_break: True adds 'break' statement at the end of |body|.
            likeliness: The likeliness of |body|.
        """
        if case is not None:
            case = _to_maybe_text_node(case)
            case.set_outer(self)
        body = _to_symbol_scope_node(body, likeliness)
        body.set_outer(self)

        if case is not None:
            self._clauses.append(self._Clause(case, body, should_add_break))
        else:
            assert not self._default_clauses
            self._default_clauses.append(
                self._Clause(case, body, should_add_break))


class CxxForLoopNode(CompositeNode):
    def __init__(self, cond, body, weak_dep_syms=None):
        assert weak_dep_syms is None or isinstance(weak_dep_syms,
                                                   (list, tuple))

        if weak_dep_syms is None:
            weak_deps = EmptyNode()
        else:
            weak_deps = WeakDependencyNode(weak_dep_syms)

        template_format = (
            "{weak_deps}"  #
            "for ({cond}) {{\n"
            "  {body}\n"
            "}}\n")

        CompositeNode.__init__(self,
                               template_format,
                               weak_deps=weak_deps,
                               cond=_to_conditional_node(cond),
                               body=_to_symbol_scope_node(
                                   body, Likeliness.LIKELY))


class CxxBreakableBlockNode(CompositeNode):
    def __init__(self, body, likeliness=Likeliness.LIKELY):
        template_format = ("do {{  // Dummy loop for use of 'break'.\n"
                           "  {body}\n"
                           "}} while (false);")

        CompositeNode.__init__(
            self,
            template_format,
            body=_to_symbol_scope_node(body, likeliness))


class CxxFuncDeclNode(CompositeNode):
    def __init__(self,
                 name,
                 arg_decls,
                 return_type,
                 template_params=None,
                 static=False,
                 explicit=False,
                 constexpr=False,
                 const=False,
                 override=False,
                 default=False,
                 delete=False,
                 nodiscard=False):
        """
        Args:
            name: Function name.
            arg_decls: List of argument declarations.
            return_type: Return type.
            template_params: List of template parameters or None.
            static: True makes this a static function.
            explicit: True makes this an explicit constructor.
            constexpr: True makes this a constexpr function.
            const: True makes this a const function.
            override: True makes this an overriding function.
            default: True makes this have the default implementation.
            delete: True makes this function be deleted.
            nodiscard: True adds [[nodiscard]] attribute.
        """
        assert isinstance(name, str)
        assert isinstance(static, bool)
        assert isinstance(explicit, bool)
        assert isinstance(constexpr, bool)
        assert isinstance(const, bool)
        assert isinstance(override, bool)
        assert isinstance(default, bool)
        assert isinstance(delete, bool)
        assert not (default and delete)
        assert isinstance(nodiscard, bool)

        template_format = ("{template}"
                           "{nodiscard_result}"
                           "{static}{explicit}{constexpr}"
                           "{return_type} "
                           "{name}({arg_decls})"
                           "{const}"
                           "{override}"
                           "{default_or_delete}"
                           ";")

        if template_params is None:
            template = ""
        else:
            template = "template <{}>\n".format(", ".join(template_params))
        static = "static " if static else ""
        explicit = "explicit " if explicit else ""
        constexpr = "constexpr " if constexpr else ""
        const = " const" if const else ""
        override = " override" if override else ""
        if default:
            default_or_delete = " = default"
        elif delete:
            default_or_delete = " = delete"
        else:
            default_or_delete = ""
        nodiscard_result = ("[[nodiscard]] " if nodiscard else "")

        CompositeNode.__init__(self,
                               template_format,
                               name=_to_maybe_text_node(name),
                               arg_decls=ListNode(map(_to_maybe_text_node,
                                                      arg_decls),
                                                  separator=", "),
                               return_type=_to_maybe_text_node(return_type),
                               template=template,
                               static=static,
                               explicit=explicit,
                               constexpr=constexpr,
                               const=const,
                               override=override,
                               default_or_delete=default_or_delete,
                               nodiscard_result=nodiscard_result)


class CxxFuncDefNode(CompositeNode):
    def __init__(self,
                 name,
                 arg_decls,
                 return_type,
                 class_name=None,
                 template_params=None,
                 static=False,
                 inline=False,
                 explicit=False,
                 constexpr=False,
                 const=False,
                 override=False,
                 member_initializer_list=None):
        """
        Args:
            name: Function name.
            arg_decls: List of argument declarations.
            return_type: Return type.
            class_name: Class name to be used as nested-name-specifier.
            template_params: List of template parameters or None.
            static: True makes this a static function.
            inline: True makes this an inline function.
            explicit: True makes this an explicit constructor.
            constexpr: True makes this a constexpr function.
            const: True makes this a const function.
            override: True makes this an overriding function.
            member_initializer_list: List of member initializers.
        """
        assert isinstance(name, str)
        assert isinstance(static, bool)
        assert isinstance(inline, bool)
        assert isinstance(explicit, bool)
        assert isinstance(constexpr, bool)
        assert isinstance(const, bool)
        assert isinstance(override, bool)

        self._function_name = name
        self._arg_decls = arg_decls
        self._return_type = return_type
        self._const = const

        # Presence of some attributes only makes sense on inline defitintions,
        # in which case a separate declaration does not make sense.
        self._inhibit_make_decl = (template_params or inline or explicit
                                   or constexpr)

        template_format = ("{template}"
                           "{static}{inline}{explicit}{constexpr}"
                           "{return_type} "
                           "{class_name}{name}({arg_decls})"
                           "{const}"
                           "{override}"
                           "{member_initializer_list} {{\n"
                           "  {body}\n"
                           "}}")

        if class_name is None:
            class_name = ""
        else:
            class_name = ListNode([_to_maybe_text_node(class_name)], tail="::")

        if template_params is None:
            template = ""
        else:
            template = "template <{}>\n".format(", ".join(template_params))

        static = "static " if static else ""
        inline = "inline " if inline else ""
        explicit = "explicit " if explicit else ""
        constexpr = "constexpr " if constexpr else ""
        const = " const" if const else ""
        override = " override" if override else ""

        if member_initializer_list is None:
            member_initializer_list = ""
        else:
            member_initializer_list = ListNode(
                map(_to_maybe_text_node, member_initializer_list),
                separator=", ",
                head=" : ")

        self._body_node = SymbolScopeNode()

        CompositeNode.__init__(
            self,
            template_format,
            name=_to_maybe_text_node(name),
            arg_decls=ListNode(
                map(_to_maybe_text_node, arg_decls), separator=", "),
            return_type=_to_maybe_text_node(return_type),
            class_name=class_name,
            template=template,
            static=static,
            inline=inline,
            explicit=explicit,
            constexpr=constexpr,
            const=const,
            override=override,
            member_initializer_list=member_initializer_list,
            body=self._body_node)

    @property
    def function_name(self):
        return self._function_name

    @property
    def body(self):
        return self._body_node

    def make_decl(self,
                  static=False,
                  explicit=False,
                  override=False,
                  nodiscard=False):
        assert not self._inhibit_make_decl
        return CxxFuncDeclNode(name=self._function_name,
                               arg_decls=self._arg_decls,
                               return_type=self._return_type,
                               const=self._const,
                               static=static,
                               explicit=explicit,
                               override=override,
                               nodiscard=nodiscard)

class CxxClassDefNode(CompositeNode):
    def __init__(self,
                 name,
                 base_class_names=None,
                 template_params=None,
                 final=False,
                 export=None):
        """
        Args:
            name: The class name to be defined.
            base_class_names: The list of base class names.
            template_params: List of template parameters or None.
            final: True makes this a final class.
            export: Class export annotation.
        """
        assert isinstance(final, bool)

        template_format = ("{template}"
                           "class{export} {name}{final}{base_clause} {{\n"
                           "  {top_section}\n"
                           "  {public_section}\n"
                           "  {protected_section}\n"
                           "  {private_section}\n"
                           "  {bottom_section}\n"
                           "}};")

        if template_params is None:
            template = ""
        else:
            template = "template <{}>\n".format(", ".join(template_params))

        if export is None:
            export = ""
        else:
            export = ListNode([_to_maybe_text_node(export)], head=" ")

        final = " final" if final else ""

        if base_class_names is None:
            base_clause = ""
        else:
            base_specifier_list = [
                CompositeNode(
                    "public {base_class_name}",
                    base_class_name=_to_maybe_text_node(base_class_name))
                for base_class_name in base_class_names
            ]
            base_clause = ListNode(
                base_specifier_list, separator=", ", head=" : ")

        self._top_section = ListNode(tail="\n")
        self._public_section = ListNode(head="public:\n", tail="\n")
        self._protected_section = ListNode(head="protected:\n", tail="\n")
        self._private_section = ListNode(head="private:\n", tail="\n")
        self._bottom_section = ListNode()

        CompositeNode.__init__(self,
                               template_format,
                               name=_to_maybe_text_node(name),
                               base_clause=base_clause,
                               template=template,
                               final=final,
                               export=export,
                               top_section=self._top_section,
                               public_section=self._public_section,
                               protected_section=self._protected_section,
                               private_section=self._private_section,
                               bottom_section=self._bottom_section)

    @property
    def top_section(self):
        return self._top_section

    @property
    def public_section(self):
        return self._public_section

    @property
    def protected_section(self):
        return self._protected_section

    @property
    def private_section(self):
        return self._private_section

    @property
    def bottom_section(self):
        return self._bottom_section


class CxxNamespaceNode(CompositeNode):
    def __init__(self, name="", body=None):
        template_format = ("namespace {name} {{\n"
                           "\n"
                           "{body}\n"
                           "\n"
                           "}}  // namespace {name}")

        if body is None:
            self._body = ListNode()
        else:
            self._body = _to_list_node(body)

        CompositeNode.__init__(
            self, template_format, name=name, body=self._body)

    @property
    def body(self):
        return self._body


def _to_conditional_node(cond):
    if isinstance(cond, CodeNode):
        return cond
    if isinstance(cond, CodeGenExpr):
        return TextNode(cond.to_text())
    if isinstance(cond, str):
        return TextNode(cond)
    assert False


def _to_list_node(node):
    if isinstance(node, ListNode):
        return node
    if isinstance(node, CodeNode):
        return ListNode([node])
    if isinstance(node, (list, tuple)):
        return ListNode(node)
    assert False


def _to_maybe_text_node(node):
    if isinstance(node, CodeNode):
        return node
    if isinstance(node, str):
        return TextNode(node)
    assert False


def _to_symbol_scope_node(node, likeliness):
    if isinstance(node, SymbolScopeNode):
        pass
    elif isinstance(node, CodeNode):
        node = SymbolScopeNode([node])
    elif isinstance(node, (list, tuple)):
        node = SymbolScopeNode(node)
    else:
        assert False
    node.set_likeliness(likeliness)
    return node

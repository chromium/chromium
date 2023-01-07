# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Lint for IDL """

import os
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_visitor import IDLVisitor


Option('wcomment', 'Disable warning for missing comment.')
Option('wenum', 'Disable warning for missing enum value.')
Option('winline', 'Disable warning for inline blocks.')
Option('wname', 'Disable warning for inconsistent interface name.')
Option('wnone', 'Disable all warnings.')
Option('wparam', 'Disable warning for missing [in|out|inout] on param.')
Option('wpass', 'Disable warning for mixed passByValue and returnByValue.')

#
# IDLLinter
#
# Once the AST is build, we need to resolve the namespace and version
# information.
#
class IDLLinter(IDLVisitor):
  def VisitFilter(self, node, data):
    __pychecker__ = 'unusednames=node,data'
    return not node.IsA('Comment', 'Copyright')

  def Arrive(self, node, errors):
    __pychecker__ = 'unusednames=node,errors'
    warnings = 0
    if node.IsA('Interface', 'Member', 'Struct', 'Enum', 'EnumItem', 'Typedef'):
      comments = node.GetListOf('Comment')
      if not comments and not node.GetProperty('wcomment'):
        node.Warning('Expecting a comment.')
        warnings += 1

    if node.IsA('File'):
      labels = node.GetListOf('Label')
      interfaces = node.GetListOf('Interface')
      if interfaces and not labels:
        node.Warning('Expecting a label in a file containing interfaces.')

    if node.IsA('Struct', 'Typedef') and not node.GetProperty('wpass'):
      if node.GetProperty('passByValue'):
        pbv = 'is'
      else:
        pbv = 'is not'
      if node.GetProperty('returnByValue'):
        ret = 'is'
      else:
        ret = 'is not'
      if pbv != ret:
        node.Warning('%s passByValue but %s returnByValue.' % (pbv, ret))
        warnings += 1

    if node.IsA('EnumItem'):
      if not node.GetProperty('VALUE') and not node.GetProperty('wenum'):
        node.Warning('Expecting value for enumeration.')
        warnings += 1

    if node.IsA('Interface'):
      macro = node.GetProperty('macro')
      if macro and not node.GetProperty('wname'):
        node.Warning('Interface name inconsistent: %s' % macro)
        warnings += 1

    if node.IsA('Inline') and not node.GetProperty('winline'):
      inline_type = node.GetProperty('NAME')
      node.parent.Warning('Requires an inline %s block.' % inline_type)
      warnings += 1

    if node.IsA('Callspec') and not node.GetProperty('wparam'):
      out = False
      for arg in node.GetListOf('Param'):
        if arg.GetProperty('out'):
          out = True
        if arg.GetProperty('in') and out:
          arg.Warning('[in] parameter after [out] parameter')
          warnings += 1

    if node.IsA('Param') and not node.GetProperty('wparam'):
      found = False;
      for form in ['in', 'inout', 'out']:
        if node.GetProperty(form): found = True
      if not found:
        node.Warning('Missing argument type: [in|out|inout]')
        warnings += 1

    return warnings

  def Depart(self, node, warnings, childdata):
    __pychecker__ = 'unusednames=node'
    for child in childdata:
      warnings += child
    return warnings

def Lint(ast):
  options = ['wcomment', 'wenum', 'winline', 'wparam', 'wpass', 'wname']
  wnone = GetOption('wnone')
  for opt in options:
    if wnone or GetOption(opt): ast.SetProperty(opt, True)

  skipList = []
  for filenode in ast.GetListOf('File'):
    name = filenode.GetProperty('NAME')
    if filenode.GetProperty('ERRORS') > 0:
      ErrOut.Log('%s : Skipped due to errors.' % name)
      skipList.append(filenode)
      continue
    warnings = IDLLinter().Visit(filenode, 0)
    if warnings:
      WarnOut.Log('%s warning(s) for %s\n' % (warnings, name))
  return skipList

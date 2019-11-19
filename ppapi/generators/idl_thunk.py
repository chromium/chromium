#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" Generator for C++ style thunks """

from __future__ import print_function

import glob
import os
import re
import sys

from idl_log import ErrOut, InfoOut, WarnOut
from idl_node import IDLAttribute, IDLNode
from idl_ast import IDLAst
from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
from idl_parser import ParseFiles
from idl_c_proto import CGen, GetNodeComments, CommentLines, Comment
from idl_generator import Generator, GeneratorByFile

Option('thunkroot', 'Base directory of output',
       default=os.path.join('..', 'thunk'))


class TGenError(Exception):
  def __init__(self, msg):
    self.value = msg

  def __str__(self):
    return repr(self.value)


class ThunkBodyMetadata(object):
  """Metadata about thunk body. Used for selecting which headers to emit."""
  def __init__(self):
    self._apis = set()
    self._builtin_includes = set()
    self._includes = set()

  def AddApi(self, api):
    self._apis.add(api)

  def Apis(self):
    return self._apis

  def AddInclude(self, include):
    self._includes.add(include)

  def Includes(self):
    return self._includes

  def AddBuiltinInclude(self, include):
    self._builtin_includes.add(include)

  def BuiltinIncludes(self):
    return self._builtin_includes


def _GetBaseFileName(filenode):
  """Returns the base name for output files, given the filenode.

  Examples:
    'dev/ppb_find_dev.h' -> 'ppb_find_dev'
    'trusted/ppb_buffer_trusted.h' -> 'ppb_buffer_trusted'
  """
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0]
  return name


def _GetHeaderFileName(filenode):
  """Returns the name for the header for this file."""
  path, name = os.path.split(filenode.GetProperty('NAME'))
  name = os.path.splitext(name)[0]
  if path:
    header = "ppapi/c/%s/%s.h" % (path, name)
  else:
    header = "ppapi/c/%s.h" % name
  return header


def _GetThunkFileName(filenode, relpath):
  """Returns the thunk file name."""
  path = os.path.split(filenode.GetProperty('NAME'))[0]
  name = _GetBaseFileName(filenode)
  # We don't reattach the path for thunk.
  if relpath: name = os.path.join(relpath, name)
  name = '%s%s' % (name, '_thunk.cc')
  return name


def _StripFileName(filenode):
  """Strips path  and dev, trusted, and private suffixes from the file name."""
  api_basename = _GetBaseFileName(filenode)
  if api_basename.endswith('_dev'):
    api_basename = api_basename[:-len('_dev')]
  if api_basename.endswith('_trusted'):
    api_basename = api_basename[:-len('_trusted')]
  if api_basename.endswith('_private'):
    api_basename = api_basename[:-len('_private')]
  return api_basename


def _StripApiName(api_name):
  """Strips Dev, Private, and Trusted suffixes from the API name."""
  if api_name.endswith('Trusted'):
    api_name = api_name[:-len('Trusted')]
  if api_name.endswith('_Dev'):
    api_name = api_name[:-len('_Dev')]
  if api_name.endswith('_Private'):
    api_name = api_name[:-len('_Private')]
  return api_name


def _MakeEnterLine(filenode, interface, member, arg, handle_errors, callback,
                   meta):
  """Returns an EnterInstance/EnterResource string for a function."""
  api_name = _StripApiName(interface.GetName()) + '_API'
  if member.GetProperty('api'):  # Override API name.
    manually_provided_api = True
    # TODO(teravest): Automatically guess the API header file.
    api_name = member.GetProperty('api')
  else:
    manually_provided_api = False

  if arg[0] == 'PP_Instance':
    if callback is None:
      arg_string = arg[1]
    else:
      arg_string = '%s, %s' % (arg[1], callback)
    if interface.GetProperty('singleton') or member.GetProperty('singleton'):
      if not manually_provided_api:
        meta.AddApi('ppapi/thunk/%s_api.h' % _StripFileName(filenode))
      return 'EnterInstanceAPI<%s> enter(%s);' % (api_name, arg_string)
    else:
      return 'EnterInstance enter(%s);' % arg_string
  elif arg[0] == 'PP_Resource':
    enter_type = 'EnterResource<%s>' % api_name
    if not manually_provided_api:
      meta.AddApi('ppapi/thunk/%s_api.h' % _StripFileName(filenode))
    if callback is None:
      return '%s enter(%s, %s);' % (enter_type, arg[1],
                                    str(handle_errors).lower())
    else:
      return '%s enter(%s, %s, %s);' % (enter_type, arg[1],
                                        callback,
                                        str(handle_errors).lower())
  else:
    raise TGenError("Unknown type for _MakeEnterLine: %s" % arg[0])


def _GetShortName(interface, filter_suffixes):
  """Return a shorter interface name that matches Is* and Create* functions."""
  parts = interface.GetName().split('_')[1:]
  tail = parts[len(parts) - 1]
  if tail in filter_suffixes:
    parts = parts[:-1]
  return ''.join(parts)


def _IsTypeCheck(interface, node, args):
  """Returns true if node represents a type-checking function."""
  if len(args) == 0 or args[0][0] != 'PP_Resource':
    return False
  return node.GetName() == 'Is%s' % _GetShortName(interface, ['Dev', 'Private'])


def _GetCreateFuncName(interface):
  """Returns the creation function name for an interface."""
  return 'Create%s' % _GetShortName(interface, ['Dev'])


def _GetDefaultFailureValue(t):
  """Returns the default failure value for a given type.

  Returns None if no default failure value exists for the type.
  """
  values = {
      'PP_Bool': 'PP_FALSE',
      'PP_Resource': '0',
      'struct PP_Var': 'PP_MakeUndefined()',
      'float': '0.0f',
      'int32_t': 'enter.retval()',
      'uint16_t': '0',
      'uint32_t': '0',
      'uint64_t': '0',
      'void*': 'NULL'
  }
  if t in values:
    return values[t]
  return None


def _MakeCreateMemberBody(interface, member, args):
  """Returns the body of a Create() function.

  Args:
    interface - IDLNode for the interface
    member - IDLNode for member function
    args - List of arguments for the Create() function
  """
  if args[0][0] == 'PP_Resource':
    body = 'Resource* object =\n'
    body += '    PpapiGlobals::Get()->GetResourceTracker()->'
    body += 'GetResource(%s);\n' % args[0][1]
    body += 'if (!object)\n'
    body += '  return 0;\n'
    body += 'EnterResourceCreation enter(object->pp_instance());\n'
  elif args[0][0] == 'PP_Instance':
    body = 'EnterResourceCreation enter(%s);\n' % args[0][1]
  else:
    raise TGenError('Unknown arg type for Create(): %s' % args[0][0])

  body += 'if (enter.failed())\n'
  body += '  return 0;\n'
  arg_list = ', '.join([a[1] for a in args])
  if member.GetProperty('create_func'):
    create_func = member.GetProperty('create_func')
  else:
    create_func = _GetCreateFuncName(interface)
  body += 'return enter.functions()->%s(%s);' % (create_func,
                                                 arg_list)
  return body


def _GetOutputParams(member, release):
  """Returns output parameters (and their types) for a member function.

  Args:
    member - IDLNode for the member function
    release - Release to get output parameters for
  Returns:
    A list of name strings for all output parameters of the member
    function.
  """
  out_params = []
  callnode = member.GetOneOf('Callspec')
  if callnode:
    cgen = CGen()
    for param in callnode.GetListOf('Param'):
      mode = cgen.GetParamMode(param)
      if mode == 'out':
        # We use the 'store' mode when getting the parameter type, since we
        # need to call sizeof() for memset().
        _, pname, _, _ = cgen.GetComponents(param, release, 'store')
        out_params.append(pname)
  return out_params


def _MakeNormalMemberBody(filenode, release, node, member, rtype, args,
                          include_version, meta):
  """Returns the body of a typical function.

  Args:
    filenode - IDLNode for the file
    release - release to generate body for
    node - IDLNode for the interface
    member - IDLNode for the member function
    rtype - Return type for the member function
    args - List of 4-tuple arguments for the member function
    include_version - whether to include the version in the invocation
    meta - ThunkBodyMetadata for header hints
  """
  if len(args) == 0:
    # Calling into the "Shared" code for the interface seems like a reasonable
    # heuristic when we don't have any arguments; some thunk code follows this
    # convention today.
    meta.AddApi('ppapi/shared_impl/%s_shared.h' % _StripFileName(filenode))
    return 'return %s::%s();' % (_StripApiName(node.GetName()) + '_Shared',
                                 member.GetName())

  is_callback_func = args[len(args) - 1][0] == 'struct PP_CompletionCallback'

  if is_callback_func:
    call_args = args[:-1] + [('', 'enter.callback()', '', '')]
    meta.AddInclude('ppapi/c/pp_completion_callback.h')
  else:
    call_args = args

  if args[0][0] == 'PP_Instance':
    call_arglist = ', '.join(a[1] for a in call_args)
    function_container = 'functions'
  elif args[0][0] == 'PP_Resource':
    call_arglist = ', '.join(a[1] for a in call_args[1:])
    function_container = 'object'
  else:
    # Calling into the "Shared" code for the interface seems like a reasonable
    # heuristic when the first argument isn't a PP_Instance or a PP_Resource;
    # some thunk code follows this convention today.
    meta.AddApi('ppapi/shared_impl/%s_shared.h' % _StripFileName(filenode))
    return 'return %s::%s(%s);' % (_StripApiName(node.GetName()) + '_Shared',
                                   member.GetName(),
                                   ', '.join(a[1] for a in args))

  function_name = member.GetName()
  if include_version:
    version = node.GetVersion(release).replace('.', '_')
    function_name += version

  invocation = 'enter.%s()->%s(%s)' % (function_container,
                                       function_name,
                                       call_arglist)

  handle_errors = not (member.GetProperty('report_errors') == 'False')
  out_params = _GetOutputParams(member, release)
  if is_callback_func:
    body = '%s\n' % _MakeEnterLine(filenode, node, member, args[0],
                                   handle_errors, args[len(args) - 1][1], meta)
    failure_value = member.GetProperty('on_failure')
    if failure_value is None:
      failure_value = 'enter.retval()'
    failure_return = 'return %s;' % failure_value
    success_return = 'return enter.SetResult(%s);' % invocation
  elif rtype == 'void':
    body = '%s\n' % _MakeEnterLine(filenode, node, member, args[0],
                                   handle_errors, None, meta)
    failure_return = 'return;'
    success_return = '%s;' % invocation  # We don't return anything for void.
  else:
    body = '%s\n' % _MakeEnterLine(filenode, node, member, args[0],
                                   handle_errors, None, meta)
    failure_value = member.GetProperty('on_failure')
    if failure_value is None:
      failure_value = _GetDefaultFailureValue(rtype)
    if failure_value is None:
      raise TGenError('There is no default value for rtype %s. '
                      'Maybe you should provide an on_failure attribute '
                      'in the IDL file.' % rtype)
    failure_return = 'return %s;' % failure_value
    success_return = 'return %s;' % invocation

  if member.GetProperty('always_set_output_parameters'):
    body += 'if (enter.failed()) {\n'
    for param in out_params:
      body += '  memset(%s, 0, sizeof(*%s));\n' % (param, param)
    body += '  %s\n' % failure_return
    body += '}\n'
    body += '%s' % success_return
    meta.AddBuiltinInclude('string.h')
  else:
    body += 'if (enter.failed())\n'
    body += '  %s\n' % failure_return
    body += '%s' % success_return
  return body


def DefineMember(filenode, node, member, release, include_version, meta):
  """Returns a definition for a member function of an interface.

  Args:
    filenode - IDLNode for the file
    node - IDLNode for the interface
    member - IDLNode for the member function
    release - release to generate
    include_version - include the version in emitted function name.
    meta - ThunkMetadata for header hints
  Returns:
    A string with the member definition.
  """
  cgen = CGen()
  rtype, name, arrays, args = cgen.GetComponents(member, release, 'return')
  log_body = '\"%s::%s()\";' % (node.GetName(),
                                cgen.GetStructName(member, release,
                                                   include_version))
  if len(log_body) > 69:  # Prevent lines over 80 characters.
    body = 'VLOG(4) <<\n'
    body += '    %s\n' % log_body
  else:
    body = 'VLOG(4) << %s\n' % log_body

  if _IsTypeCheck(node, member, args):
    body += '%s\n' % _MakeEnterLine(filenode, node, member, args[0], False,
                                    None, meta)
    body += 'return PP_FromBool(enter.succeeded());'
  elif member.GetName() == 'Create' or member.GetName() == 'CreateTrusted':
    body += _MakeCreateMemberBody(node, member, args)
  else:
    body += _MakeNormalMemberBody(filenode, release, node, member, rtype, args,
                                  include_version, meta)

  signature = cgen.GetSignature(member, release, 'return', func_as_ptr=False,
                                include_version=include_version)
  return '%s\n%s\n}' % (cgen.Indent('%s {' % signature, tabs=0),
                        cgen.Indent(body, tabs=1))


def _IsNewestMember(member, members, releases):
  """Returns true if member is the newest node with its name in members.

  Currently, every node in the AST only has one version. This means that we
  will have two sibling nodes with the same name to represent different
  versions.
  See http://crbug.com/157017 .

  Special handling is required for nodes which share their name with others,
  but aren't the newest version in the IDL.

  Args:
    member - The member which is checked if it's newest
    members - The list of members to inspect
    releases - The set of releases to check for versions in.
  """
  build_list = member.GetUniqueReleases(releases)
  release = build_list[0]  # Pick the oldest release.
  same_name_siblings = filter(
      lambda n: str(n) == str(member) and n != member, members)

  for s in same_name_siblings:
    sibling_build_list = s.GetUniqueReleases(releases)
    sibling_release = sibling_build_list[0]
    if sibling_release > release:
      return False
  return True


class TGen(GeneratorByFile):
  def __init__(self):
    Generator.__init__(self, 'Thunk', 'tgen', 'Generate the C++ thunk.')

  def GenerateFile(self, filenode, releases, options):
    savename = _GetThunkFileName(filenode, GetOption('thunkroot'))
    my_min, my_max = filenode.GetMinMax(releases)
    if my_min > releases[-1] or my_max < releases[0]:
      if os.path.isfile(savename):
        print("Removing stale %s for this range." % filenode.GetName())
        os.remove(os.path.realpath(savename))
      return False
    do_generate = filenode.GetProperty('generate_thunk')
    if not do_generate:
      return False

    thunk_out = IDLOutFile(savename)
    body, meta = self.GenerateBody(thunk_out, filenode, releases, options)
    # TODO(teravest): How do we handle repeated values?
    if filenode.GetProperty('thunk_include'):
      meta.AddInclude(filenode.GetProperty('thunk_include'))
    self.WriteHead(thunk_out, filenode, releases, options, meta)
    thunk_out.Write('\n\n'.join(body))
    self.WriteTail(thunk_out, filenode, releases, options)
    thunk_out.ClangFormat()
    return thunk_out.Close()

  def WriteHead(self, out, filenode, releases, options, meta):
    __pychecker__ = 'unusednames=options'
    cgen = CGen()

    cright_node = filenode.GetChildren()[0]
    assert(cright_node.IsA('Copyright'))
    out.Write('%s\n' % cgen.Copyright(cright_node, cpp_style=True))

    from_text = 'From %s' % (
        filenode.GetProperty('NAME').replace(os.sep,'/'))
    modified_text = 'modified %s.' % (
        filenode.GetProperty('DATETIME'))
    out.Write('// %s %s\n\n' % (from_text, modified_text))

    meta.AddBuiltinInclude('stdint.h')
    if meta.BuiltinIncludes():
      for include in sorted(meta.BuiltinIncludes()):
        out.Write('#include <%s>\n' % include)
      out.Write('\n')

    # TODO(teravest): Don't emit includes we don't need.
    includes = ['ppapi/c/pp_errors.h',
                'ppapi/shared_impl/tracked_callback.h',
                'ppapi/thunk/enter.h',
                'ppapi/thunk/ppapi_thunk_export.h']
    includes.append(_GetHeaderFileName(filenode))
    for api in meta.Apis():
      includes.append('%s' % api.lower())
    for i in meta.Includes():
      includes.append(i)
    for include in sorted(includes):
      out.Write('#include "%s"\n' % include)
    out.Write('\n')
    out.Write('namespace ppapi {\n')
    out.Write('namespace thunk {\n')
    out.Write('\n')
    out.Write('namespace {\n')
    out.Write('\n')

  def GenerateBody(self, out, filenode, releases, options):
    """Generates a member function lines to be written and metadata.

    Returns a tuple of (body, meta) where:
      body - a list of lines with member function bodies
      meta - a ThunkMetadata instance for hinting which headers are needed.
    """
    __pychecker__ = 'unusednames=options'
    out_members = []
    meta = ThunkBodyMetadata()
    for node in filenode.GetListOf('Interface'):
      # Skip if this node is not in this release
      if not node.InReleases(releases):
        print("Skipping %s" % node)
        continue

      # Generate Member functions
      if node.IsA('Interface'):
        members = node.GetListOf('Member')
        for child in members:
          build_list = child.GetUniqueReleases(releases)
          # We have to filter out releases this node isn't in.
          build_list = filter(lambda r: child.InReleases([r]), build_list)
          if len(build_list) == 0:
            continue
          release = build_list[-1]
          include_version = not _IsNewestMember(child, members, releases)
          member = DefineMember(filenode, node, child, release, include_version,
                                meta)
          if not member:
            continue
          out_members.append(member)
    return (out_members, meta)

  def WriteTail(self, out, filenode, releases, options):
    __pychecker__ = 'unusednames=options'
    cgen = CGen()

    version_list = []
    out.Write('\n\n')
    for node in filenode.GetListOf('Interface'):
      build_list = node.GetUniqueReleases(releases)
      for build in build_list:
        version = node.GetVersion(build).replace('.', '_')
        thunk_name = 'g_' + node.GetName().lower() + '_thunk_' + \
                      version
        thunk_type = '_'.join((node.GetName(), version))
        version_list.append((thunk_type, thunk_name))

        out.Write('const %s %s = {\n' % (thunk_type, thunk_name))
        generated_functions = []
        members = node.GetListOf('Member')
        for child in members:
          rtype, name, arrays, args = cgen.GetComponents(
              child, build, 'return')
          if child.InReleases([build]):
            if not _IsNewestMember(child, members, releases):
              version = child.GetVersion(
                  child.first_release[build]).replace('.', '_')
              name += '_' + version
            generated_functions.append(name)
        out.Write(',\n'.join(['  &%s' % f for f in generated_functions]))
        out.Write('\n};\n\n')

    out.Write('}  // namespace\n')
    out.Write('\n')
    for thunk_type, thunk_name in version_list:
      out.Write('PPAPI_THUNK_EXPORT const %s* Get%s_Thunk() {\n' %
                    (thunk_type, thunk_type))
      out.Write('  return &%s;\n' % thunk_name)
      out.Write('}\n')
      out.Write('\n')
    out.Write('}  // namespace thunk\n')
    out.Write('}  // namespace ppapi\n')


tgen = TGen()


def Main(args):
  # Default invocation will verify the golden files are unchanged.
  failed = 0
  if not args:
    args = ['--wnone', '--diff', '--test', '--thunkroot=.']

  ParseOptions(args)

  idldir = os.path.split(sys.argv[0])[0]
  idldir = os.path.join(idldir, 'test_thunk', '*.idl')
  filenames = glob.glob(idldir)
  ast = ParseFiles(filenames)
  if tgen.GenerateRange(ast, ['M13', 'M14', 'M15'], {}):
    print("Golden file for M13-M15 failed.")
    failed = 1
  else:
    print("Golden file for M13-M15 passed.")

  return failed


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

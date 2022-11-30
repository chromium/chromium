#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generator for Pnacl Shim functions that bridges the calling conventions
between GCC and PNaCl.  """

from datetime import datetime
import difflib
import glob
import os
import sys

from idl_c_proto import CGen
from idl_gen_wrapper import Interface, WrapperGen
from idl_log import ErrOut, InfoOut, WarnOut
from idl_option import GetOption, Option, ParseOptions
from idl_parser import ParseFiles

Option('pnaclshim', 'Name of the pnacl shim file.',
       default='temp_pnacl_shim.c')

Option('disable_pnacl_opt', 'Turn off optimization of pnacl shim.')


class PnaclGen(WrapperGen):
  """PnaclGen generates shim code to bridge the Gcc ABI with PNaCl.

  This subclass of WrapperGenerator takes the IDL sources and
  generates shim methods for bridging the calling conventions between GCC
  and PNaCl (LLVM). Some of the PPAPI methods do not need shimming, so
  this will also detect those situations and provide direct access to the
  original PPAPI methods (rather than the shim methods).
  """

  def __init__(self):
    WrapperGen.__init__(self,
                        'Pnacl',
                        'Pnacl Shim Gen',
                        'pnacl',
                        'Generate the PNaCl shim.')
    self.cgen = CGen()
    self._skip_opt = False

  ############################################################

  def OwnHeaderFile(self):
    """Return the header file that specifies the API of this wrapper.
    We do not generate the header files.  """
    return 'ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.h'


  def InterfaceVersionNeedsWrapping(self, iface, version):
    """Return true if the interface+version has ANY methods that
    need wrapping.
    """
    if self._skip_opt:
      return True
    if iface.GetName().endswith('Trusted'):
      return False
    # TODO(dmichael): We have no way to wrap PPP_ interfaces without an
    # interface string. If any ever need wrapping, we'll need to figure out a
    # way to get the plugin-side of the Pepper proxy (within the IRT) to access
    # and use the wrapper.
    if iface.GetProperty("no_interface_string"):
      return False
    for member in iface.GetListOf('Member'):
      release = member.GetRelease(version)
      if self.MemberNeedsWrapping(member, release):
        return True
    return False


  def MemberNeedsWrapping(self, member, release):
    """Return true if a particular member function at a particular
    release needs wrapping.
    """
    if self._skip_opt:
      return True
    if not member.InReleases([release]):
      return False
    ret, name, array, args_spec = self.cgen.GetComponents(member,
                                                          release,
                                                          'store')
    return self.TypeNeedsWrapping(ret, []) or self.ArgsNeedWrapping(args_spec)


  def ArgsNeedWrapping(self, args):
    """Return true if any parameter in the list needs wrapping.
    """
    for arg in args:
      (type_str, name, array_dims, more_args) = arg
      if self.TypeNeedsWrapping(type_str, array_dims):
        return True
    return False


  def TypeNeedsWrapping(self, type_node, array_dims):
    """Return true if a parameter type needs wrapping.
    Currently, this is true for byval aggregates.
    """
    is_aggregate = type_node.startswith('struct') or \
        type_node.startswith('union')
    is_reference = (type_node.find('*') != -1 or array_dims != [])
    return is_aggregate and not is_reference

  ############################################################


  def ConvertByValueReturnType(self, ret, args_spec):
    if self.TypeNeedsWrapping(ret, array_dims=[]):
      args_spec = [(ret, '_struct_result', [], None)] + args_spec
      ret2 = 'void'
      wrap_return = True
    else:
      ret2 = ret
      wrap_return = False
    return wrap_return, ret2, args_spec


  def ConvertByValueArguments(self, args_spec):
    args = []
    for type_str, name, array_dims, more_args in args_spec:
      if self.TypeNeedsWrapping(type_str, array_dims):
        type_str += '*'
      args.append((type_str, name, array_dims, more_args))
    return args


  def FormatArgs(self, c_operator, args_spec):
    args = []
    for type_str, name, array_dims, more_args in args_spec:
      if self.TypeNeedsWrapping(type_str, array_dims):
        args.append(c_operator + name)
      else:
        args.append(name)
    return ', '.join(args)


  def GenerateWrapperForPPBMethod(self, iface, member):
    result = []
    func_prefix = self.WrapperMethodPrefix(iface.node, iface.release)
    ret, name, array, cspec = self.cgen.GetComponents(member,
                                                      iface.release,
                                                      'store')
    wrap_return, ret2, cspec2 = self.ConvertByValueReturnType(ret, cspec)
    cspec2 = self.ConvertByValueArguments(cspec2)
    sig = self.cgen.Compose(ret2, name, array, cspec2,
                            prefix=func_prefix,
                            func_as_ptr=False,
                            include_name=True,
                            unsized_as_ptr=False)
    result.append('static %s {\n' % sig)
    result.append('  const struct %s *iface = %s.real_iface;\n' %
                  (iface.struct_name, self.GetWrapperInfoName(iface)))

    return_prefix = ''
    if wrap_return:
      return_prefix = '*_struct_result = '
    elif ret != 'void':
      return_prefix = 'return '

    result.append('  %siface->%s(%s);\n}\n\n' % (return_prefix,
                                                 member.GetName(),
                                                 self.FormatArgs('*', cspec)))
    return result


  def GenerateWrapperForPPPMethod(self, iface, member):
    result = []
    func_prefix = self.WrapperMethodPrefix(iface.node, iface.release)
    sig = self.cgen.GetSignature(member, iface.release, 'store',
                                 func_prefix, False)
    result.append('static %s {\n' % sig)
    result.append('  const struct %s *iface = %s.real_iface;\n' %
                  (iface.struct_name, self.GetWrapperInfoName(iface)))
    ret, name, array, cspec = self.cgen.GetComponents(member,
                                                      iface.release,
                                                      'store')
    wrap_return, ret2, cspec = self.ConvertByValueReturnType(ret, cspec)
    cspec2 = self.ConvertByValueArguments(cspec)
    temp_fp = self.cgen.Compose(ret2, name, array, cspec2,
                                prefix='temp_fp',
                                func_as_ptr=True,
                                include_name=False,
                                unsized_as_ptr=False)
    cast = self.cgen.Compose(ret2, name, array, cspec2,
                             prefix='',
                             func_as_ptr=True,
                             include_name=False,
                             unsized_as_ptr=False)
    result.append('  %s =\n    ((%s)iface->%s);\n' % (temp_fp,
                                                      cast,
                                                      member.GetName()))
    return_prefix = ''
    if wrap_return:
      result.append('  %s _struct_result;\n' % ret)
    elif ret != 'void':
      return_prefix = 'return '

    result.append('  %stemp_fp(%s);\n' % (return_prefix,
                                          self.FormatArgs('&', cspec)))
    if wrap_return:
      result.append('  return _struct_result;\n')
    result.append('}\n\n')
    return result


  def GenerateRange(self, ast, releases, options):
    """Generate shim code for a range of releases.
    """
    self._skip_opt = GetOption('disable_pnacl_opt')
    self.SetOutputFile(GetOption('pnaclshim'))
    return WrapperGen.GenerateRange(self, ast, releases, options)

pnaclgen = PnaclGen()

######################################################################
# Tests.

# Clean a string representing an object definition and return then string
# as a single space delimited set of tokens.
def CleanString(instr):
  instr = instr.strip()
  instr = instr.split()
  return ' '.join(instr)


def PrintErrorDiff(old, new):
  oldlines = old.split(';')
  newlines = new.split(';')
  d = difflib.Differ()
  diff = d.compare(oldlines, newlines)
  ErrOut.Log('Diff is:\n%s' % '\n'.join(diff))


def GetOldTestOutput(ast):
  # Scan the top-level comments in the IDL file for comparison.
  old = []
  for filenode in ast.GetListOf('File'):
    for node in filenode.GetChildren():
      instr = node.GetOneOf('Comment')
      if not instr: continue
      instr.Dump()
      old.append(instr.GetName())
  return CleanString(''.join(old))


def TestFiles(filenames, test_releases):
  ast = ParseFiles(filenames)
  iface_releases = pnaclgen.DetermineInterfaces(ast, test_releases)
  new_output = CleanString(pnaclgen.GenerateWrapperForMethods(
      iface_releases, comments=False))
  old_output = GetOldTestOutput(ast)
  if new_output != old_output:
    PrintErrorDiff(old_output, new_output)
    ErrOut.Log('Failed pnacl generator test.')
    return 1
  else:
    InfoOut.Log('Passed pnacl generator test.')
    return 0


def Main(args):
  filenames = ParseOptions(args)
  test_releases = ['M13', 'M14', 'M15']
  if not filenames:
    idldir = os.path.split(sys.argv[0])[0]
    idldir = os.path.join(idldir, 'test_gen_pnacl', '*.idl')
    filenames = glob.glob(idldir)
  filenames = sorted(filenames)
  if GetOption('test'):
    # Run the tests.
    return TestFiles(filenames, test_releases)

  # Otherwise, generate the output file (for potential use as golden file).
  ast = ParseFiles(filenames)
  return pnaclgen.GenerateRange(ast, test_releases, filenames)


if __name__ == '__main__':
  retval = Main(sys.argv[1:])
  sys.exit(retval)

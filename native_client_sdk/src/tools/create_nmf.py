#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool for automatically creating .nmf files from .nexe/.pexe/.bc executables.

As well as creating the nmf file this tool can also find and stage
any shared libraries dependencies that the executables might have.
"""

import argparse
import errno
import json
import os
import posixpath
import shutil
import sys

import getos

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LIB_DIR = os.path.join(SCRIPT_DIR, 'lib')

sys.path.append(LIB_DIR)

import elf
import get_shared_deps
import quote


ARCH_LOCATION = {
    'x86-32': 'lib32',
    'x86-64': 'lib64',
    'arm': 'libarm',
}


# These constants are used within nmf files.
MAIN_NEXE = 'main.nexe'  # Name of entry point for execution
PROGRAM_KEY = 'program'  # Key of the program section in an nmf file
URL_KEY = 'url'  # Key of the url field for a particular file in an nmf file
FILES_KEY = 'files'  # Key of the files section in an nmf file
PNACL_OPTLEVEL_KEY = 'optlevel' # key for PNaCl optimization level
PORTABLE_KEY = 'portable' # key for portable section of manifest
TRANSLATE_KEY = 'pnacl-translate' # key for translatable objects
TRANSLATE_DEBUG_KEY = 'pnacl-debug' # key for translatable debug objects


def DebugPrint(message):
  if DebugPrint.debug_mode:
    sys.stderr.write('%s\n' % message)


DebugPrint.debug_mode = False  # Set to True to enable extra debug prints


def SplitPath(path):
  """Returns all components of a path as a list.

  e.g.
  'foo/bar/baz.blah' => ['foo', 'bar', 'baz.blah']
  """
  result = []
  while path:
    path, part = os.path.split(path)
    result.append(part)
  return result[::-1]  # Reverse.


def MakePosixPath(path):
  """Converts from the native format to posixpath format.

  e.g. on Windows, "foo\\bar\\baz.blah" => "foo/bar/baz.blah"
  on Mac/Linux this is a no-op.
  """
  if os.path == posixpath:
    return path
  return posixpath.join(*SplitPath(path))


def PosixRelPath(path, start):
  """Takes two paths in native format, and produces a relative path in posix
  format.

  e.g.
  For Windows: "foo\\bar\\baz.blah", "foo" => "bar/baz.blah"
  For Mac/Linux: "foo/bar/baz.blah", "foo" => "bar/baz.blah"

  NOTE: This function uses os.path.realpath to create a canonical path for
  |path| and |start|.
  """
  real_path = os.path.realpath(path)
  real_start = os.path.realpath(start)
  return MakePosixPath(os.path.relpath(real_path, real_start))


def DirectoryTreeContainsFile(dirname, filename):
  """Returns True if a file is in a directory, or any of that directory's
  subdirectories recursively.

  e.g.
  DirectoryTreeContainsFile("foo", "foo/quux.txt") => True
  DirectoryTreeContainsFile("foo", "foo/bar/baz/blah.txt") => True
  DirectoryTreeContainsFile("foo", "bar/blah.txt") => False
  """
  real_dirname = os.path.realpath(dirname)
  real_filename = os.path.realpath(filename)
  return real_filename.startswith(real_dirname)


def MakeDir(dirname):
  """Just like os.makedirs but doesn't generate errors when dirname
  already exists.
  """
  if os.path.isdir(dirname):
    return

  Trace("mkdir: %s" % dirname)
  try:
    os.makedirs(dirname)
  except OSError as exception_info:
    if exception_info.errno != errno.EEXIST:
      raise


def ParseElfHeader(path):
  """Wrap elf.ParseElfHeader to return raise this module's Error on failure."""
  try:
    return elf.ParseElfHeader(path)
  except elf.Error as e:
    raise Error(str(e))


class Error(Exception):
  """Local Error class for this file."""
  pass


def IsLoader(filename):
  return (filename.endswith(get_shared_deps.LOADER_X86) or
      filename.endswith(get_shared_deps.LOADER_ARM))


class ArchFile(object):
  """Simple structure containing information about an architecture-specific
     file.

  Attributes:
    name: Name of this file
    path: Full path to this file on the build system
    arch: Architecture of this file (e.g., x86-32)
    url: Relative path to file in the staged web directory.
        Used for specifying the "url" attribute in the nmf file."""

  def __init__(self, name, path, url=None, arch=None):
    self.name = name
    self.path = path
    self.url = url
    self.arch = arch
    if arch is None:
      self.arch = ParseElfHeader(path)[0]

  def __repr__(self):
    return '<ArchFile %s>' % self.path

  def __str__(self):
    """Return the file path when invoked with the str() function"""
    return self.path


class NmfUtils(object):
  """Helper class for creating and managing nmf files"""

  def __init__(self, main_files=None, objdump=None,
               lib_path=None, extra_files=None, lib_prefix=None,
               nexe_prefix=None, no_arch_prefix=None, remap=None,
               pnacl_optlevel=None, pnacl_debug_optlevel=None,
               nmf_root=None):
    """Constructor

    Args:
      main_files: List of main entry program files.  These will be named
          files->main.nexe for dynamic nexes, and program for static nexes
      objdump: path to x86_64-nacl-objdump tool (or Linux equivalent)
      lib_path: List of paths to library directories
      extra_files: List of extra files to include in the nmf
      lib_prefix: A path prefix to prepend to the library paths, both for
          staging the libraries and for inclusion into the nmf file.
          Example: '../lib_dir'
      nexe_prefix: Like lib_prefix, but is prepended to the nexes instead.
      no_arch_prefix: Don't prefix shared libraries by lib32/lib64.
      remap: Remaps the library name in the manifest.
      pnacl_optlevel: Optimization level for PNaCl translation.
      pnacl_debug_optlevel: Optimization level for debug PNaCl translation.
      nmf_root: Directory of the NMF. All urls are relative to this directory.
    """
    assert len(main_files) > 0
    self.objdump = objdump
    self.main_files = main_files
    self.extra_files = extra_files or []
    self.lib_path = lib_path or []
    self.manifest = None
    self.needed = None
    self.lib_prefix = lib_prefix or ''
    self.nexe_prefix = nexe_prefix or ''
    self.no_arch_prefix = no_arch_prefix
    self.remap = remap or {}
    self.pnacl = main_files[0].endswith(('.pexe', '.bc'))
    self.pnacl_optlevel = pnacl_optlevel
    self.pnacl_debug_optlevel = pnacl_debug_optlevel
    if nmf_root is not None:
      self.nmf_root = nmf_root
    else:
      # To match old behavior, if there is no nmf_root, use the directory of
      # the first nexe found in main_files.
      self.nmf_root = os.path.dirname(main_files[0])

    for filename in self.main_files:
      if not os.path.exists(filename):
        raise Error('Input file not found: %s' % filename)
      if not os.path.isfile(filename):
        raise Error('Input is not a file: %s' % filename)

  def GetNeeded(self):
    """Collect the list of dependencies for the main_files

    Returns:
      A dict with key=filename and value=ArchFile of input files.
          Includes the input files as well, with arch filled in if absent.
          Example: { '/path/to/my.nexe': ArchFile(my.nexe),
                     '/path/to/libfoo.so': ArchFile(libfoo.so) }"""

    if self.needed:
      return self.needed

    DebugPrint('GetNeeded(%s)' % self.main_files)

    if not self.objdump:
      self.objdump = FindObjdumpExecutable()

    try:
      all_files = get_shared_deps.GetNeeded(self.main_files, self.objdump,
                                            self.lib_path)
    except get_shared_deps.NoObjdumpError:
      raise Error('No objdump executable found (see --help for more info)')
    except get_shared_deps.Error as e:
      raise Error(str(e))

    self.needed = {}

    # all_files is a dictionary mapping filename to architecture. self.needed
    # should be a dictionary of filename to ArchFile.
    for filename, arch in all_files.items():
      name = os.path.basename(filename)
      self.needed[filename] = ArchFile(name=name, path=filename, arch=arch)

    self._SetArchFileUrls()

    return self.needed

  def _SetArchFileUrls(self):
    """Fill in the url member of all ArchFiles in self.needed.

    All urls are relative to the nmf_root. In addition, architecture-specific
    files are relative to the .nexe with the matching architecture. This is
    useful when making a multi-platform packaged app, so each architecture's
    files are in a different directory.
    """
    # self.GetNeeded() should have already been called.
    assert self.needed is not None

    main_nexes = [f for f in self.main_files if f.endswith('.nexe')]

    # map from each arch to its corresponding main nexe.
    arch_to_main_dir = {}
    for main_file in main_nexes:
      arch, _ = ParseElfHeader(main_file)
      main_dir = os.path.dirname(main_file)
      main_dir = PosixRelPath(main_dir, self.nmf_root)
      if main_dir == '.':
        main_dir = ''
      arch_to_main_dir[arch] = main_dir

    for arch_file in self.needed.values():
      prefix = ''
      if DirectoryTreeContainsFile(self.nmf_root, arch_file.path):
        # This file is already in the nmf_root tree, so it does not need to be
        # staged. Just make the URL relative to the .nmf.
        url = PosixRelPath(arch_file.path, self.nmf_root)
      else:
        # This file is outside of the nmf_root subtree, so it needs to be
        # staged. Its path should be relative to the main .nexe with the same
        # architecture.
        prefix = arch_to_main_dir[arch_file.arch]
        url = os.path.basename(arch_file.path)

      if arch_file.name.endswith('.nexe') and not IsLoader(arch_file.name):
        prefix = posixpath.join(prefix, self.nexe_prefix)
      elif self.no_arch_prefix:
        prefix = posixpath.join(prefix, self.lib_prefix)
      else:
        prefix = posixpath.join(
            prefix, self.lib_prefix, ARCH_LOCATION[arch_file.arch])
      arch_file.url = posixpath.join(prefix, url)

  def StageDependencies(self, destination_dir):
    """Copies over the dependencies into a given destination directory

    Each library will be put into a subdirectory that corresponds to the arch.

    Args:
      destination_dir: The destination directory for staging the dependencies
    """
    assert self.needed is not None
    for arch_file in self.needed.values():
      source = arch_file.path
      destination = os.path.join(destination_dir, arch_file.url)

      if (os.path.normcase(os.path.realpath(source)) ==
          os.path.normcase(os.path.realpath(destination))):
        continue

      # make sure target dir exists
      MakeDir(os.path.dirname(destination))

      Trace('copy: %s -> %s' % (source, destination))
      shutil.copy2(source, destination)

  def _GeneratePNaClManifest(self):
    manifest = {}
    manifest[PROGRAM_KEY] = {}
    manifest[PROGRAM_KEY][PORTABLE_KEY] = {}
    portable = manifest[PROGRAM_KEY][PORTABLE_KEY]
    for filename in self.main_files:
      translate_dict =  {
          'url': os.path.basename(filename),
      }
      if filename.endswith('.pexe'):
        if self.pnacl_optlevel is not None:
          translate_dict[PNACL_OPTLEVEL_KEY] = self.pnacl_optlevel
        if TRANSLATE_KEY in portable:
          raise Error('Multiple .pexe files')
        portable[TRANSLATE_KEY] = translate_dict
      elif filename.endswith('.bc'):
        if self.pnacl_debug_optlevel is not None:
          translate_dict[PNACL_OPTLEVEL_KEY] = self.pnacl_debug_optlevel
        if TRANSLATE_DEBUG_KEY in portable:
          raise Error('Multiple .bc files')
        portable[TRANSLATE_DEBUG_KEY] = translate_dict
      else:
        raise Error('Unexpected executable type: %s' % filename)
    self.manifest = manifest

  def _GenerateManifest(self):
    """Create a JSON formatted dict containing the files

    NaCl will map url requests based on architecture.  The startup NEXE
    can always be found under the top key PROGRAM.  Additional files are under
    the FILES key further mapped by file name.  In the case of 'runnable' the
    PROGRAM key is populated with urls pointing the runnable-ld.so which acts
    as the startup nexe.  The application itself is then placed under the
    FILES key mapped as 'main.exe' instead of the original name so that the
    loader can find it.
    """
    manifest = { FILES_KEY: {}, PROGRAM_KEY: {} }

    needed = self.GetNeeded()

    extra_files_kv = [(key, ArchFile(name=key,
                                     arch=arch,
                                     path=url,
                                     url=url))
                      for key, arch, url in self.extra_files]

    manifest_items = list(needed.items()) + extra_files_kv

    # Add dynamic loader to the program section.
    for need, archinfo in manifest_items:
      if IsLoader(need):
        urlinfo = { URL_KEY: archinfo.url }
        manifest[PROGRAM_KEY][archinfo.arch] = urlinfo

    for need, archinfo in manifest_items:
      urlinfo = { URL_KEY: archinfo.url }
      name = archinfo.name
      arch = archinfo.arch

      if IsLoader(need):
        continue

      if need in self.main_files:
        if need.endswith(".nexe"):
          # Place it under program if we aren't using the runnable-ld.so.
          program = manifest[PROGRAM_KEY]
          if arch not in program:
            program[arch] = urlinfo
            continue
          # Otherwise, treat it like another another file named main.nexe.
          name = MAIN_NEXE

      name = self.remap.get(name, name)
      fileinfo = manifest[FILES_KEY].get(name, {})
      fileinfo[arch] = urlinfo
      manifest[FILES_KEY][name] = fileinfo
    self.manifest = manifest

  def GetManifest(self):
    """Returns a JSON-formatted dict containing the NaCl dependencies"""
    if self.manifest is None:
      if self.pnacl:
        self._GeneratePNaClManifest()
      else:
        self._GenerateManifest()
    return self.manifest

  def GetJson(self):
    """Returns the Manifest as a JSON-formatted string"""
    pretty_string = json.dumps(self.GetManifest(), indent=2)
    # json.dumps sometimes returns trailing whitespace and does not put
    # a newline at the end.  This code fixes these problems.
    pretty_lines = pretty_string.split('\n')
    return '\n'.join([line.rstrip() for line in pretty_lines]) + '\n'


def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')

Trace.verbose = False


def ParseExtraFiles(encoded_list, err):
  """Parse the extra-files list and return a canonicalized list of
  [key, arch, url] triples.  The |encoded_list| should be a list of
  strings of the form 'key:url' or 'key:arch:url', where an omitted
  'arch' is taken to mean 'portable'.

  All entries in |encoded_list| are checked for syntax errors before
  returning.  Error messages are written to |err| (typically
  sys.stderr) so that the user has actionable feedback for fixing all
  errors, rather than one at a time.  If there are any errors, None is
  returned instead of a list, since an empty list is a valid return
  value.
  """
  seen_error = False
  canonicalized = []
  for ix in range(len(encoded_list)):
    kv = encoded_list[ix]
    unquoted = quote.unquote(kv, ':')
    if len(unquoted) == 3:
      if unquoted[1] != ':':
        err.write('Syntax error for key:value tuple ' +
                  'for --extra-files argument: ' + kv + '\n')
        seen_error = True
      else:
        canonicalized.append([unquoted[0], 'portable', unquoted[2]])
    elif len(unquoted) == 5:
      if unquoted[1] != ':' or unquoted[3] != ':':
        err.write('Syntax error for key:arch:url tuple ' +
                  'for --extra-files argument: ' +
                  kv + '\n')
        seen_error = True
      else:
        canonicalized.append([unquoted[0], unquoted[2], unquoted[4]])
    else:
      err.write('Bad key:arch:url tuple for --extra-files: ' + kv + '\n')
  if seen_error:
    return None
  return canonicalized


def GetSDKRoot():
  """Returns the root directory of the NaCl SDK.
  """
  # This script should be installed in NACL_SDK_ROOT/tools. Assert that
  # the 'toolchain' folder exists within this directory in case, for
  # example, this script is moved to a different location.
  # During the Chrome build this script is sometimes run outside of
  # of an SDK but in these cases it should always be run with --objdump=
  # and --no-default-libpath which avoids the need to call this function.
  sdk_root = os.path.dirname(SCRIPT_DIR)
  assert(os.path.exists(os.path.join(sdk_root, 'toolchain')))
  return sdk_root


def FindObjdumpExecutable():
  """Derive path to objdump executable to use for determining shared
  object dependencies.
  """
  osname = getos.GetPlatform()
  toolchain = os.path.join(GetSDKRoot(), 'toolchain', '%s_x86_glibc' % osname)
  objdump = os.path.join(toolchain, 'bin', 'x86_64-nacl-objdump')
  if osname == 'win':
    objdump += '.exe'

  if not os.path.exists(objdump):
    sys.stderr.write('WARNING: failed to find objdump in default '
                     'location: %s' % objdump)
    return None

  return objdump


def GetDefaultLibPath(config):
  """Derive default library path.

  This path is used when searching for shared objects.  This currently includes
  the toolchain library folders and the top level SDK lib folder.
  """
  sdk_root = GetSDKRoot()

  osname = getos.GetPlatform()
  libpath = [
    # Core toolchain libraries
    'toolchain/%s_x86_glibc/x86_64-nacl/lib' % osname,
    'toolchain/%s_x86_glibc/x86_64-nacl/lib32' % osname,
    'toolchain/%s_arm_glibc/arm-nacl/lib' % osname,
    # user installed libraries (used by webports)
    'toolchain/%s_x86_glibc/x86_64-nacl/usr/lib' % osname,
    'toolchain/%s_x86_glibc/i686-nacl/usr/lib' % osname,
    'toolchain/%s_arm_glibc/arm-nacl/usr/lib' % osname,
    # SDK bundle libraries
    'lib/glibc_x86_32/%s' % config,
    'lib/glibc_x86_64/%s' % config,
    'lib/glibc_arm/%s' % config,
  ]

  # In some cases (e.g. ASAN, TSAN, STANDALONE) the name of the configuration
  # can be different to simply Debug or Release.  For example 'msan_Release'.
  # In this case we search for libraries first in this directory and then
  # fall back to 'Release'.
  if config not in ['Debug', 'Release']:
    config_fallback = 'Release'
    if 'Debug' in config:
      config_fallback = 'Debug'

    libpath += [
      'lib/glibc_x86_32/%s' % config_fallback,
      'lib/glibc_x86_64/%s' % config_fallback,
      'lib/glibc_arm/%s' % config_fallback,
      'ports/lib/glibc_x86_32/%s' % config_fallback,
      'ports/lib/glibc_x86_64/%s' % config_fallback,
      'ports/lib/glibc_arm/%s' % config_fallback,
    ]

  libpath = [os.path.normpath(p) for p in libpath]
  libpath = [os.path.join(sdk_root, p) for p in libpath]
  libpath.append(os.path.join(sdk_root, 'tools'))
  return libpath


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-o', '--output', dest='output',
                      help='Write manifest file to FILE (default is stdout)',
                      metavar='FILE')
  parser.add_argument('-D', '--objdump', dest='objdump',
                      help='Override the default "objdump" tool used to find '
                           'shared object dependencies',
                      metavar='TOOL')
  parser.add_argument('--no-default-libpath', action='store_true',
                      help="Don't include the SDK default library paths")
  parser.add_argument('--debug-libs', action='store_true',
                      help='Legacy option, do not use')
  parser.add_argument('--config', default='Release',
                      help='Use a particular library configuration (normally '
                           'Debug or Release)')
  parser.add_argument('-L', '--library-path', dest='lib_path',
                      action='append', default=[],
                      help='Add DIRECTORY to library search path',
                      metavar='DIRECTORY')
  parser.add_argument('-P', '--path-prefix', dest='path_prefix', default='',
                      help='Deprecated. An alias for --lib-prefix.',
                      metavar='DIRECTORY')
  parser.add_argument('-p', '--lib-prefix', dest='lib_prefix', default='',
                      help='A path to prepend to shared libraries in the .nmf',
                      metavar='DIRECTORY')
  parser.add_argument('-N', '--nexe-prefix', dest='nexe_prefix', default='',
                      help='A path to prepend to nexes in the .nmf',
                      metavar='DIRECTORY')
  parser.add_argument('-s', '--stage-dependencies', dest='stage_dependencies',
                      help='Destination directory for staging libraries',
                      metavar='DIRECTORY')
  parser.add_argument('--no-arch-prefix', action='store_true',
                      help='Don\'t put shared libraries in the lib32/lib64 '
                      'directories. Instead, they will be put in the same '
                      'directory as the .nexe that matches its architecture.')
  parser.add_argument('-t', '--toolchain', help='Legacy option, do not use')
  parser.add_argument('-n', '--name', dest='name',
                      help='Rename FOO as BAR',
                      action='append', default=[], metavar='FOO,BAR')
  parser.add_argument('-x', '--extra-files',
                      help='Add extra key:file tuple to the "files"'
                           ' section of the .nmf',
                      action='append', default=[], metavar='FILE')
  parser.add_argument('-O', '--pnacl-optlevel',
                      help='Set the optimization level to N in PNaCl manifests',
                      metavar='N')
  parser.add_argument('--pnacl-debug-optlevel',
                      help='Set the optimization level to N for debugging '
                           'sections in PNaCl manifests',
                      metavar='N')
  parser.add_argument('-v', '--verbose',
                      help='Verbose output', action='store_true')
  parser.add_argument('-d', '--debug-mode',
                      help='Debug mode', action='store_true')
  parser.add_argument('executables', metavar='EXECUTABLE', nargs='+')

  # To enable bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete create_nmf.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(args)
  if options.verbose:
    Trace.verbose = True
  if options.debug_mode:
    DebugPrint.debug_mode = True

  if options.toolchain is not None:
    sys.stderr.write('warning: option -t/--toolchain is deprecated.\n')
  if options.debug_libs:
    sys.stderr.write('warning: --debug-libs is deprecated (use --config).\n')
    # Implement legacy behavior
    options.config = 'Debug'

  canonicalized = ParseExtraFiles(options.extra_files, sys.stderr)
  if canonicalized is None:
    parser.error('Bad --extra-files (-x) argument syntax')

  remap = {}
  for ren in options.name:
    parts = ren.split(',')
    if len(parts) != 2:
      parser.error('Expecting --name=<orig_arch.so>,<new_name.so>')
    remap[parts[0]] = parts[1]

  if options.path_prefix:
    options.lib_prefix = options.path_prefix

  for libpath in options.lib_path:
    if not os.path.exists(libpath):
      sys.stderr.write('Specified library path does not exist: %s\n' % libpath)
    elif not os.path.isdir(libpath):
      sys.stderr.write('Specified library is not a directory: %s\n' % libpath)

  if not options.no_default_libpath:
    # Add default libraries paths to the end of the search path.
    options.lib_path += GetDefaultLibPath(options.config)
    for path in options.lib_path:
      Trace('libpath: %s' % path)

  pnacl_optlevel = None
  if options.pnacl_optlevel is not None:
    pnacl_optlevel = int(options.pnacl_optlevel)
    if pnacl_optlevel < 0 or pnacl_optlevel > 3:
      sys.stderr.write(
          'warning: PNaCl optlevel %d is unsupported (< 0 or > 3)\n' %
          pnacl_optlevel)
  if options.pnacl_debug_optlevel is not None:
    pnacl_debug_optlevel = int(options.pnacl_debug_optlevel)
  else:
    pnacl_debug_optlevel = pnacl_optlevel

  nmf_root = None
  if options.output:
    nmf_root = os.path.dirname(options.output)

  nmf = NmfUtils(objdump=options.objdump,
                 main_files=options.executables,
                 lib_path=options.lib_path,
                 extra_files=canonicalized,
                 lib_prefix=options.lib_prefix,
                 nexe_prefix=options.nexe_prefix,
                 no_arch_prefix=options.no_arch_prefix,
                 remap=remap,
                 pnacl_optlevel=pnacl_optlevel,
                 pnacl_debug_optlevel=pnacl_debug_optlevel,
                 nmf_root=nmf_root)

  if options.output is None:
    sys.stdout.write(nmf.GetJson())
  else:
    with open(options.output, 'w') as output:
      output.write(nmf.GetJson())

  if options.stage_dependencies and not nmf.pnacl:
    Trace('Staging dependencies...')
    nmf.StageDependencies(options.stage_dependencies)

  return 0


if __name__ == '__main__':
  try:
    rtn = main(sys.argv[1:])
  except Error as e:
    sys.stderr.write('%s: %s\n' % (os.path.basename(__file__), e))
    rtn = 1
  except KeyboardInterrupt:
    sys.stderr.write('%s: interrupted\n' % os.path.basename(__file__))
    rtn = 1
  sys.exit(rtn)

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to close over all transitive dependencies of a given .nexe
executable.

e.g. Given
A -> B
B -> C
B -> D
C -> E

where "A -> B" means A depends on B, then GetNeeded(A) will return A, B, C, D
and E.
"""

import os
import re
import subprocess

import elf

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))

NeededMatcher = re.compile('^ *NEEDED *([^ ]+)\n$')
FormatMatcher = re.compile('^(.+):\\s*file format (.+)\n$')

LOADER_X86 = 'runnable-ld.so'  # Name of the dynamic loader
LOADER_ARM = 'elf_loader_arm.nexe'  # Name of the ARM dynamic loader

OBJDUMP_ARCH_MAP = {
    # Names returned by Linux's objdump:
    'elf64-x86-64': 'x86-64',
    'elf32-i386': 'x86-32',
    'elf32-little': 'arm',
    'elf32-littlearm': 'arm',
    # Names returned by old x86_64-nacl-objdump:
    'elf64-nacl': 'x86-64',
    'elf32-nacl': 'x86-32',
    # Names returned by new x86_64-nacl-objdump:
    'elf64-x86-64-nacl': 'x86-64',
    'elf32-x86-64-nacl': 'x86-64',
    'elf32-i386-nacl': 'x86-32',
    'elf32-littlearm-nacl': 'arm',
}

# The proper name of the dynamic linker, as kept in the IRT.  This is
# excluded from the nmf file by convention.
LD_NACL_MAP = {
    'x86-32': 'ld-nacl-x86-32.so.1',
    'x86-64': 'ld-nacl-x86-64.so.1',
    'arm': None,
}


class Error(Exception):
  '''Local Error class for this file.'''
  pass


class NoObjdumpError(Error):
  '''Error raised when objdump is needed but not found'''
  pass


def GetNeeded(main_files, objdump, lib_path):
  '''Collect the list of dependencies for the main_files

  Args:
    main_files: A list of files to find dependencies of.
    objdump: Path to the objdump executable.
    lib_path: A list of paths to search for shared libraries.

  Returns:
    A dict with key=filename and value=architecture. The architecture will be
    one of ('x86_32', 'x86_64', 'arm').
  '''

  dynamic = any(elf.ParseElfHeader(f)[1] for f in main_files)

  if dynamic:
    return _GetNeededDynamic(main_files, objdump, lib_path)
  else:
    return _GetNeededStatic(main_files)


def _GetNeededDynamic(main_files, objdump, lib_path):
  examined = set()
  all_files, unexamined = GleanFromObjdump(main_files, None, objdump, lib_path)
  for arch in all_files.values():
    if unexamined:
      if arch == 'arm':
        unexamined.add((LOADER_ARM, arch))
      else:
        unexamined.add((LOADER_X86, arch))

  while unexamined:
    files_to_examine = {}

    # Take all the currently unexamined files and group them
    # by architecture.
    for name, arch in unexamined:
      files_to_examine.setdefault(arch, []).append(name)

    # Call GleanFromObjdump() for each architecture.
    needed = set()
    for arch, files in files_to_examine.items():
      new_files, new_needed = GleanFromObjdump(files, arch, objdump, lib_path)
      all_files.update(new_files)
      needed |= new_needed

    examined |= unexamined
    unexamined = needed - examined

  # With the runnable-ld.so scheme we have today, the proper name of
  # the dynamic linker should be excluded from the list of files.
  ldso = [LD_NACL_MAP[arch] for arch in set(OBJDUMP_ARCH_MAP.values())]
  for filename, arch in list(all_files.items()):
    name = os.path.basename(filename)
    if name in ldso:
      del all_files[filename]

  return all_files


def GleanFromObjdump(files, arch, objdump, lib_path):
  '''Get architecture and dependency information for given files

  Args:
    files: A list of files to examine.
        [ '/path/to/my.nexe',
          '/path/to/lib64/libmy.so',
          '/path/to/mydata.so',
          '/path/to/my.data' ]
    arch: The architecure we are looking for, or None to accept any
          architecture.
    objdump: Path to the objdump executable.
    lib_path: A list of paths to search for shared libraries.

  Returns: A tuple with the following members:
    input_info: A dict with key=filename and value=architecture. The
        architecture will be one of ('x86_32', 'x86_64', 'arm').
    needed: A set of strings formatted as "arch/name".  Example:
        set(['x86-32/libc.so', 'x86-64/libgcc.so'])
  '''
  if not objdump:
    raise NoObjdumpError('No objdump executable found!')

  full_paths = set()
  for filename in files:
    if os.path.exists(filename):
      full_paths.add(filename)
    else:
      for path in _FindLibsInPath(filename, lib_path):
        full_paths.add(path)

  cmd = [objdump, '-p'] + list(sorted(full_paths))
  env = {'LANG': 'en_US.UTF-8'}
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, bufsize=-1,
                          universal_newlines=True,
                          env=env)

  input_info = {}
  found_basenames = set()
  needed = set()
  output, err_output = proc.communicate()
  if proc.returncode:
    raise Error('%s\nStdError=%s\nobjdump failed with error code: %d' %
                (output, err_output, proc.returncode))

  file_arch = None
  for line in output.splitlines(True):
    # Objdump should display the architecture first and then the dependencies
    # second for each file in the list.
    matched = FormatMatcher.match(line)
    if matched:
      filename = matched.group(1)
      file_arch = OBJDUMP_ARCH_MAP[matched.group(2)]
      if arch and file_arch != arch:
        continue
      name = os.path.basename(filename)
      found_basenames.add(name)
      input_info[filename] = file_arch
    matched = NeededMatcher.match(line)
    if matched:
      if arch and file_arch != arch:
        continue
      filename = matched.group(1)
      new_needed = (filename, file_arch)
      needed.add(new_needed)

  for filename in files:
    if os.path.basename(filename) not in found_basenames:
      raise Error('Library not found [%s]: %s' % (arch, filename))

  return input_info, needed


def _FindLibsInPath(name, lib_path):
  '''Finds the set of libraries matching |name| within lib_path

  Args:
    name: name of library to find
    lib_path: A list of paths to search for shared libraries.

  Returns:
    A list of system paths that match the given name within the lib_path'''
  files = []
  for dirname in lib_path:
    # The libc.so files in the the glibc toolchain is actually a linker
    # script which references libc.so.<SHA1>.  This means the libc.so itself
    # does not end up in the NEEDED section for glibc.
    if name == 'libc.so':
      continue
    filename = os.path.join(dirname, name)
    if os.path.exists(filename):
      files.append(filename)
  if not files:
    raise Error('cannot find library %s' % name)
  return files


def _GetNeededStatic(main_files):
  needed = {}
  for filename in main_files:
    arch = elf.ParseElfHeader(filename)[0]
    needed[filename] = arch
  return needed

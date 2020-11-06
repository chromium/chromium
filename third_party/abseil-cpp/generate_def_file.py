#!/usr/bin/env python

"""Script to generate Chromium's Abseil .def file from a GN action.

Since Abseil doesn't export symbols, Chromium is forced to consider all
Abseil's symbols as publicly visible. On POSIX it is possible to use
-fvisibility=default but on Windows a .def file with all the symbols
is needed.
"""

import fnmatch
import logging
import os
import re
import subprocess
import sys
import tempfile
import time

# Matches a mangled symbol that has 'absl' in it, this should be a good
# enough heuristic to select Abseil symbols to list in the .def file.
ABSL_SYM_RE = re.compile(r'0* [BT] (?P<symbol>(\?+)[^\?].*absl.*)')
if sys.platform == 'win32':
  # Typical dumpbin /symbol lines look like this:
  # 04B 0000000C SECT14 notype       Static       | ?$S1@?1??SetCurrent
  # ThreadIdentity@base_internal@absl@@YAXPAUThreadIdentity@12@P6AXPAX@Z@Z@4IA
  #  (unsigned int `void __cdecl absl::base_internal::SetCurrentThreadIdentity...
  # We need to start on "| ?" and end on the first " (" (stopping on space would
  # also work).
  # This regex is identical inside the () characters except for the ? after .*,
  # which is needed to prevent greedily grabbing the undecorated version of the
  # symbols.
  ABSL_SYM_RE = '.*External     \| (?P<symbol>(\?+)[^\?].*?absl.*?) \(.*'
  # Typical exported symbols in dumpbin /directives look like:
  #    /EXPORT:?kHexChar@numbers_internal@absl@@3QBDB,DATA
  ABSL_EXPORTED_RE = '.*/EXPORT:(.*),.*'


def _GenerateDefFile(output_path, arch):
  """Generates a .def file for the absl component build by reading absl object files."""
  if sys.platform == 'win32':
    env_pairs = open(arch).read()[:-2].split('\0')
    env_dict = dict([item.split('=', 1) for item in env_pairs])
    use_shell = True
  else:
    env_dict = None
    use_shell = False

  symbol_dumper = ['llvm-nm-9']
  if sys.platform == 'win32':
    symbol_dumper = ['dumpbin', '/symbols']

  obj_files = []
  for root, _dirnames, filenames in os.walk(
      os.path.join('obj', 'third_party', 'abseil-cpp', 'absl')):
    matched_files = fnmatch.filter(filenames, '*.obj')
    obj_files.extend((os.path.join(root, f) for f in matched_files))


  absl_symbols = set()
  dll_exports = set()
  if sys.platform == 'win32':
    for f in obj_files:
      # Track all of the functions exported with __declspec(dllexport) and
      # don't list them in the .def file - double-exports are not allowed. The
      # error is "lld-link: error: duplicate /export option".
      exports_out = subprocess.check_output(['dumpbin', '/directives', f],
                                            shell=use_shell,
                                            env=env_dict,
                                            cwd=os.getcwd())
      for line in exports_out.splitlines():
        line = line.decode('utf-8')
        match = re.match(ABSL_EXPORTED_RE, line)
        if match:
          dll_exports.add(match.groups()[0])
  for f in obj_files:
    stdout = subprocess.check_output(symbol_dumper + [f],
                                     shell=use_shell,
                                     env=env_dict,
                                     cwd=os.getcwd())
    for line in stdout.splitlines():
      try:
        line = line.decode('utf-8')
      except UnicodeDecodeError:
        # Due to a dumpbin bug there are sometimes invalid utf-8 characters in
        # the output. This only happens on an unimportant line so it can
        # safely and silently be skipped.
        # https://developercommunity.visualstudio.com/content/problem/1091330/dumpbin-symbols-produces-randomly-wrong-output-on.html
        continue
      match = re.match(ABSL_SYM_RE, line)
      if match:
        symbol = match.group('symbol')
        assert symbol.count(' ') == 0, ('Regex matched too much, probably got '
                                        'undecorated name as well')
        # Avoid getting names exported with dllexport, to avoid
        # "lld-link: error: duplicate /export option" on symbols such as:
        # ?kHexChar@numbers_internal@absl@@3QBDB
        if symbol in dll_exports:
          continue
        absl_symbols.add(symbol)

  with open(output_path, 'w') as f:
    f.write('EXPORTS\n')
    for s in sorted(absl_symbols):
      f.write('    {}\n'.format(s))


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.INFO)
  output_path = sys.argv[1]
  if sys.platform == 'win32':
    arch = sys.argv[2]
  else:
    arch = None

  _GenerateDefFile(output_path, arch)

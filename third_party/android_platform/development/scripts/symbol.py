#!/usr/bin/python3
#
# Copyright (C) 2013 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Module for looking up symbolic debugging information.

The information can include symbol names, offsets, and source locations.
"""

import glob
import itertools
import logging
import os
import re
import struct
import subprocess
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'build', 'android'))
from pylib import constants
from pylib.constants import host_paths

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'tools', 'python'))
from llvm_symbolizer import LLVMSymbolizer, IsValidLLVMSymbolizerTarget

from llvm_objdump import LLVMObjdumper

# WARNING: These global variables can be modified by other scripts!
SYMBOLS_DIR = constants.DIR_SOURCE_ROOT + os.sep
CHROME_SYMBOLS_DIR = None
ARCH = "arm"

_SECONDARY_ABI_OUTPUT_PATH = None

# See:
# http://bugs.python.org/issue14315
# https://hg.python.org/cpython/rev/6dd5e9556a60#l2.8
def _PatchZipFile():
  oldDecodeExtra = zipfile.ZipInfo._decodeExtra
  def decodeExtra(self):
    try:
      oldDecodeExtra(self)
    except struct.error:
      pass
  zipfile.ZipInfo._decodeExtra = decodeExtra
_PatchZipFile()


# Used by _GetApkPackageName() to extract package name from aapt dump output.
_PACKAGE_NAME_RE = re.compile(r'package: .*name=\'(\S*)\'')

# Used to speed up _GetApkPackageName() because the latter is called far too
# often at the moment. Maps APK file paths to the corresponding package name.
_package_name_cache = {}

def _GetApkPackageName(apk_path):
  """Return the package name of a given apk."""
  package_name = _package_name_cache.get(apk_path, None)
  if package_name:
    return package_name

  aapt_path = host_paths.GetAaptPath()
  aapt_output = subprocess.check_output(
      [aapt_path, 'dump', 'badging', apk_path], encoding='utf8').split('\n')
  for line in aapt_output:
    match = _PACKAGE_NAME_RE.match(line)
    if match:
      package_name = match.group(1)
      _package_name_cache[apk_path] = package_name
      logging.debug('Package name %s for %s', package_name, apk_path)
      return package_name

  return None


def _PathListJoin(prefix_list, suffix_list):
  """Returns each prefix in prefix_list joined with each suffix in suffix list.

   Args:
     prefix_list: list of path prefixes.
     suffix_list: list of path suffixes.

   Returns:
     List of paths each of which joins a prefix with a suffix.
   """
  return [
      os.path.join(prefix, suffix)
      for suffix in suffix_list for prefix in prefix_list ]


def GetCandidates(dirs, filepart, candidate_fun):
  """Returns a list of candidate filenames, sorted by modification time.

  Args:
    dirs: a list of the directory part of the pathname.
    filepart: the file part of the pathname.
    candidate_fun: a function to apply to each candidate, returns a list.

  Returns:
    A list of candidate files ordered by modification time, newest first.
  """
  candidates = _PathListJoin(dirs, [filepart])
  logging.debug('GetCandidates: prefiltered candidates = %s' % candidates)
  candidates = list(
      itertools.chain.from_iterable(map(candidate_fun, candidates)))
  candidates.sort(key=os.path.getmtime, reverse=True)
  return candidates


# Used by _GetCandidateApks() to speed up its result.
_cached_candidate_apks = None

def _GetCandidateApks():
  """Returns a list of APKs which could contain the library.

  Args:
    None

  Returns:
    list of APK file paths which could contain the library.
  """
  global _cached_candidate_apks
  if _cached_candidate_apks is not None:
    return _cached_candidate_apks

  apk_dir = os.path.join(constants.GetOutDirectory(), 'apks')
  candidates = GetCandidates([apk_dir], '*.apk', glob.glob)
  _cached_candidate_apks = candidates
  return candidates


def GetCrazyLib(apk_filename):
  """Returns the name of the first crazy library from this APK.

  Args:
    apk_filename: name of an APK file.

  Returns:
    Name of the first library which would be crazy loaded from this APK.
  """
  zip_file = zipfile.ZipFile(apk_filename, 'r')
  for filename in zip_file.namelist():
    match = re.match('lib/[^/]*/crazy.(lib.*[.]so)', filename)
    if match:
      return match.group(1)

  return None

def GetApkFromLibrary(device_library_path):
  match = re.match(r'.*/([^/]*)-[0-9]+(\/[^/]*)?\.apk$', device_library_path)
  if not match:
    return None
  return match.group(1)


def GetMatchingApks(package_name):
  """Find any APKs which match the package indicated by the device_apk_name.

  Args:
     package_name: package name of the APK on the device.

  Returns:
     A list of APK filenames which could contain the desired library.
  """
  return [apk_path for apk_path in _GetCandidateApks() if (
      _GetApkPackageName(apk_path) == package_name)]


def MapDeviceApkToLibrary(device_apk_name):
  """Provide a library name which corresponds with device_apk_name.

  Args:
    device_apk_name: name of the APK on the device.

  Returns:
    Name of the library which corresponds to that APK.
  """
  matching_apks = GetMatchingApks(device_apk_name)
  logging.debug('MapDeviceApkToLibrary: matching_apks=%s' % matching_apks)
  for matching_apk in matching_apks:
    crazy_lib = GetCrazyLib(matching_apk)
    if crazy_lib:
      return crazy_lib

  return None


def GetLibrarySearchPaths():
  """Return a list of directories where to find native shared libraries."""
  if _SECONDARY_ABI_OUTPUT_PATH:
    return _PathListJoin([_SECONDARY_ABI_OUTPUT_PATH], ['lib.unstripped', '.'])
  if CHROME_SYMBOLS_DIR:
    return [CHROME_SYMBOLS_DIR]
  output_dir = constants.GetOutDirectory()
  # GN places stripped libraries under $CHROMIUM_OUTPUT_DIR and unstripped ones
  # under $CHROMIUM_OUTPUT_OUT/lib.unstripped. Place the unstripped path before
  # to get symbols from them when they exist.
  return _PathListJoin([output_dir], ['lib.unstripped', '.'])


def GetCandidateLibraries(library_name):
  """Returns a list of candidate library filenames.

  Args:
    library_name: basename of the library to match.

  Returns:
    A list of matching library filenames for library_name.
  """
  def extant_library(filename):
    if (os.path.exists(filename) and IsValidLLVMSymbolizerTarget(filename)):
      return [filename]
    return []

  candidates = GetCandidates(
      GetLibrarySearchPaths(), library_name,
      extant_library)
  # For GN, candidates includes both stripped an unstripped libraries. Stripped
  # libraries are always newer. Explicitly look for .unstripped and sort them
  # ahead.
  candidates.sort(key=lambda c: int('unstripped' not in c))
  return candidates


def TranslateLibPath(lib):
  # The filename in the stack trace maybe an APK name rather than a library
  # name. This happens when the library was loaded directly from inside the
  # APK. If this is the case we try to figure out the library name by looking
  # for a matching APK file and finding the name of the library in contains.
  # The name of the APK file on the device is of the form
  # <package_name>-<number>.apk. The APK file on the host may have any name
  # so we look at the APK badging to see if the package name matches.
  apk = GetApkFromLibrary(lib)
  if apk is not None:
    logging.debug('TranslateLibPath: apk=%s' % apk)
    mapping = MapDeviceApkToLibrary(apk)
    if mapping:
      lib = mapping

  # SymbolInformation(lib, addr) receives lib as the path from symbols
  # root to the symbols file. This needs to be translated to point to the
  # correct .so path. If the user doesn't explicitly specify which directory to
  # use, then use the most recently updated one in one of the known directories.
  # If the .so is not found somewhere in CHROME_SYMBOLS_DIR, leave it
  # untranslated in case it is an Android symbol in SYMBOLS_DIR.
  library_name = os.path.basename(lib)

  logging.debug(
      'TranslateLibPath: lib=%s library_name=%s' % (lib, library_name))

  candidate_libraries = GetCandidateLibraries(library_name)
  logging.debug(
      'TranslateLibPath: candidate_libraries=%s' % (candidate_libraries))
  if not candidate_libraries:
    return lib

  library_path = os.path.relpath(candidate_libraries[0], SYMBOLS_DIR)
  logging.debug('TranslateLibPath: library_path=%s' % library_path)
  return library_path


def _FormatSymbolWithOffset(symbol, offset):
  if offset == 0:
    return symbol
  return "%s+%d" % (symbol, offset)


def SetSecondaryAbiOutputPath(path):
  global _SECONDARY_ABI_OUTPUT_PATH
  if _SECONDARY_ABI_OUTPUT_PATH and _SECONDARY_ABI_OUTPUT_PATH != path:
    raise Exception ('SetSecondaryAbiOutputPath() was already called with a ' +
                     'different value, previous: %s new: %s' % (
                       _SECONDARY_ABI_OUTPUT_PATH, path))
  _SECONDARY_ABI_OUTPUT_PATH = path


def SymbolInformationForSet(lib, unique_addrs, get_detailed_info,
                            cpu_arch=ARCH):
  """Look up symbol information for a set of addresses from the given library.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of hexidecimal addresses
    get_detailed_info: If True, add additional info from objdump.
    cpu_arch: Target CPU architecture.

  Returns:
    A dictionary of the form {addr: [(source_symbol, source_location,
    object_symbol_with_offset)]} where each address has a list of
    associated symbols and locations.  The list is always non-empty.

    If the function has been inlined then the list may contain
    more than one element with the symbols for the most deeply
    nested inlined location appearing first.  The list is
    always non-empty, even if no information is available.

    Usually you want to display the source_location and
    object_symbol_with_offset from the last element in the list.
  """
  if not lib:
    return None

  symbols = SYMBOLS_DIR + lib

  addr_to_line = _CallAddr2LineForSet(symbols, unique_addrs)

  if not addr_to_line:
    return None

  if get_detailed_info:
    addr_to_objdump = _CallObjdumpForSet(symbols, unique_addrs, cpu_arch)
    if not addr_to_objdump:
      return None
  else:
    addr_to_objdump = dict((addr, ("", 0)) for addr in unique_addrs)

  result = {}
  for addr in unique_addrs:
    source_info = addr_to_line.get(addr)
    if not source_info:
      source_info = [(None,None)]

    if addr in addr_to_objdump:
      (object_symbol, object_offset) = addr_to_objdump.get(addr)
      object_symbol_with_offset = _FormatSymbolWithOffset(object_symbol,
                                                          object_offset)
    else:
      object_symbol_with_offset = None

    result[addr] = [(source_symbol, source_location, object_symbol_with_offset)
        for (source_symbol, source_location) in source_info]

  return result


def _CallAddr2LineForSet(lib, unique_addrs):
  """Look up line and symbol information for a set of addresses.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of string hexidecimal addresses look up.

  Returns:
    A dictionary of the form {addr: [(symbol, file:line)]} where
    each address has a list of associated symbols and locations
    or an empty list if no symbol information was found.

    If the function has been inlined then the list may contain
    more than one element with the symbols for the most deeply
    nested inlined location appearing first.
  """
  if not lib:
    return None

  if not os.path.splitext(lib)[1] in ['', '.so', '.apk']:
    return None

  if not os.path.isfile(lib):
    return None

  sorted_addrs = sorted(unique_addrs)

  result = {}

  with LLVMSymbolizer() as llvm_symbolizer:
    for addr in sorted_addrs:
      result[addr] = llvm_symbolizer.GetSymbolInformation(lib,addr)

  return result


def _CallObjdumpForSet(lib, unique_addrs, cpu_arch):
  """Use objdump to find out the names of the containing functions.

  Args:
    lib: library (or executable) pathname containing symbols
    unique_addrs: set of string hexidecimal addresses to find the functions for.
    cpu_arch: Target CPU architecture.

  Returns:
    A dictionary of the form {addr: (string symbol, offset)}.
  """

  if not lib:
    return None

  if not os.path.exists(lib):
    return None

  result = {}

  with LLVMObjdumper() as llvm_objdumper:
    for current_address in unique_addrs:
      symbol_data = llvm_objdumper.GetSymbolInformation(lib=lib,
                                                        address=current_address,
                                                        cpu_arch=cpu_arch)
      result[current_address] = (symbol_data.symbol, symbol_data.offset)

  return result

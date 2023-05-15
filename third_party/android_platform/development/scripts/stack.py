#!/usr/bin/env python3
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

"""stack symbolizes native crash dumps."""

from __future__ import print_function

import getopt
import glob
import logging
import os
import sys

import stack_core
import subprocess
import symbol
import sys
import zipfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'build', 'android'))

from pylib import constants

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                os.pardir, os.pardir, os.pardir, os.pardir,
                                'tools', 'python'))
import llvm_symbolizer

DEFAULT_SYMROOT='/tmp/symbols'
# From: https://source.android.com/source/build-numbers.html
_ANDROID_M_MAJOR_VERSION=6

def PrintUsage():
  """Print usage and exit with error."""
  print("Usage: " + sys.argv[0] + " [options] [FILE]")
  print()
  print("Find all parts in FILE belonging to stack traces and symbolize")
  print("them: copy to stdout augmenting with source file names and line")
  print("numbers.")
  print()
  print("Not providing FILE or setting it to '-' implies reading from stdin.")
  print()
  print("  -p, --pass-through")
  print("       Copy all lines from stdin to stdout in addition to")
  print("       symbolizing relevant lines. This way ADB logcat can")
  print("       be piped through the tool to symbolize on the fly.")
  print()
  print("  --symbols-dir=path")
  print("       path to the Android OS symbols dir, such as")
  print("       =/tmp/out/target/product/dream/symbols")
  print()
  print("  --chrome-symbols-dir=path")
  print("       path to a Chrome symbols dir. E.g.: out/Debug/lib.unstripped")
  print()
  print("  --output-directory=path")
  print("       the path to the build output directory, such as out/Debug.")
  print("       Ignored if --chrome-symbols-dir is passed.")
  print()
  print("  --apks-directory=path")
  print("       Overrides the default apks directory. Useful if a bundle APKS")
  print("       file has been unzipped into a temporary directory.")
  print()
  print("  --symbols-zip=path")
  print("       the path to a symbols zip file, such as")
  print("       =dream-symbols-12345.zip")
  print()
  print("  --more-info")
  print("  --less-info")
  print("       Change the level of detail in the output.")
  print("       --more-info is slower and more verbose, but more functions")
  print("       will be fully qualified with namespace/classname and have full")
  print("       argument information. Also, the 'stack data' section will be")
  print("       printed.")
  print()
  print("  --arch=arm|arm64|x64|x86|mips")
  print("       the target architecture")
  print()
  print("  --fallback-so-file=name")
  print("     fallback to given .so file (eg. libmonochrome_64.so) instead")
  print("     of libmonochrome.so if we fail to detect the shared lib which")
  print("     is loaded from APK, this doesn't work for component build.")
  print()
  print("  --quiet")
  print("       Show less logging")
  print()
  print("  --verbose")
  print("       enable extra logging, particularly for debugging failed")
  print("       symbolization")
  sys.exit(1)


def UnzipSymbols(symbolfile, symdir=None):
  """Unzips a file to DEFAULT_SYMROOT and returns the unzipped location.

  Args:
    symbolfile: The .zip file to unzip
    symdir: Optional temporary directory to use for extraction

  Returns:
    A tuple containing (the directory into which the zip file was unzipped,
    the path to the "symbols" directory in the unzipped file).  To clean
    up, the caller can delete the first element of the tuple.

  Raises:
    SymbolDownloadException: When the unzip fails.
  """
  if not symdir:
    symdir = "%s/%s" % (DEFAULT_SYMROOT, hash(symbolfile))
  if not os.path.exists(symdir):
    os.makedirs(symdir)

  logging.info('extracting %s...', symbolfile)
  with zipfile.ZipFile(symbolfile, 'r') as zip_ref:
    zip_ref.extractall(symdir)

  return symdir


def main(argv, test_symbolizer=None):
  try:
    options, arguments = getopt.getopt(argv, "p", [
        "pass-through",
        "flush",
        "more-info",
        "less-info",
        "chrome-symbols-dir=",
        "output-directory=",
        "apks-directory=",
        "symbols-dir=",
        "symbols-zip=",
        "arch=",
        "fallback-so-file=",
        "verbose",
        "quiet",
        "help",
    ])
  except getopt.GetoptError:
    PrintUsage()

  pass_through = False
  flush = False
  zip_arg = None
  more_info = False
  fallback_so_file = None
  arch_defined = False
  apks_directory = None
  log_level = logging.INFO
  for option, value in options:
    if option == "--help":
      PrintUsage()
    elif option == "--pass-through":
      pass_through = True
    elif option == "-p":
      pass_through = True
    elif option == "--flush":
      flush = True
    elif option == "--symbols-dir":
      symbol.SYMBOLS_DIR = os.path.abspath(os.path.expanduser(value))
    elif option == "--symbols-zip":
      zip_arg = os.path.abspath(os.path.expanduser(value))
    elif option == "--arch":
      symbol.ARCH = value
      arch_defined = True
    elif option == "--chrome-symbols-dir":
      symbol.CHROME_SYMBOLS_DIR = value
    elif option == "--output-directory":
      constants.SetOutputDirectory(os.path.abspath(value))
    elif option == "--apks-directory":
      apks_directory = os.path.abspath(value)
    elif option == "--more-info":
      more_info = True
    elif option == "--less-info":
      more_info = False
    elif option == "--fallback-so-file":
      fallback_so_file = value
    elif option == "--verbose":
      log_level = logging.DEBUG
    elif option == "--quiet":
      log_level = logging.WARNING

  if len(arguments) > 1:
    PrintUsage()

  rootdir = None
  if zip_arg:
    rootdir = UnzipSymbols(zip_arg)
    for subdir, dirs, _ in os.walk(rootdir):
      if 'lib.unstripped' in dirs:
        unzipped_output_dir = subdir
        break
    constants.SetOutputDirectory(unzipped_output_dir)

  logging.basicConfig(level=log_level)
  # Do an up-front test that the output directory is known.
  if not symbol.CHROME_SYMBOLS_DIR:
    constants.CheckOutputDirectory()

  logging.info('Reading Android symbols from: %s',
               os.path.normpath(symbol.SYMBOLS_DIR))
  chrome_search_path = symbol.GetLibrarySearchPaths()
  logging.info('Searching for Chrome symbols from within: %s', ':'.join(
      (os.path.normpath(d) for d in chrome_search_path)))

  if not arguments or arguments[0] == '-':
    logging.info('Reading native crash info from stdin (symbolization starts '
                 'on the first unrelated line or EOF)')
    with llvm_symbolizer.LLVMSymbolizer() as symbolizer:
      stack_core.StreamingConvertTrace(sys.stdin, {}, more_info,
                                       fallback_so_file, arch_defined,
                                       symbolizer, apks_directory, pass_through,
                                       flush)
  else:
    logging.info('Searching for native crashes in: %s',
                 os.path.realpath(arguments[0]))
    if pass_through:
      logging.error('Processing files in --pass-through mode is not supported')
      return 1

    f = open(arguments[0], 'r')

    lines = f.readlines()
    f.close()

    # This used to be required when ELF logical addresses did not align with
    # physical addresses, which happened when relocations were converted to APS2
    # format post-link via relocation_packer tool.
    load_vaddrs = {}

    with llvm_symbolizer.LLVMSymbolizer() as symbolizer:
      logging.info('Searching for Chrome symbols from within: %s', ':'.join(
          (os.path.normpath(d) for d in chrome_search_path)))
      stack_core.ConvertTrace(lines, load_vaddrs, more_info, fallback_so_file,
                              arch_defined, test_symbolizer or symbolizer,
                              apks_directory)

  if rootdir:
    # be a good citizen and clean up...os.rmdir and os.removedirs() don't work
    cmd = "rm -rf \"%s\"" % rootdir
    logging.info('cleaning up (%s)', cmd)
    os.system(cmd)

  return 0


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))

# vi: ts=2 sw=2

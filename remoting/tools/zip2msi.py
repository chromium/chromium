#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates .msi from a .zip archive or an unpacked directory.

The structure of the input archive or directory should look like this:

  +- archive.zip
     +- archive
        +- parameters.json

The name of the archive and the top level directory in the archive must match.
When an unpacked directory is used as the input "archive.zip/archive" should
be passed via the command line.

'parameters.json' specifies the parameters to be passed to candle/light and
must have the following structure:

  {
    "defines": { "name": "value" },
    "extensions": [ "WixFirewallExtension.dll" ],
    "switches": [ '-nologo' ],
    "source": "chromoting.wxs",
    "bind_path": "files",
    "sign": [ ... ],
    "candle": { ... },
    "light": { ... }
  }

"source" specifies the name of the input .wxs relative to
    "archive.zip/archive".
"bind_path" specifies the path where to look for binary files referenced by
    .wxs relative to "archive.zip/archive".

This script is used for both building Chromoting Host installation during
Chromuim build and for signing Chromoting Host installation later. There are two
copies of this script because of that:

  - one in Chromium tree at src/remoting/tools/zip2msi.py.
  - another one next to the signing scripts.

The copies of the script can be out of sync so make sure that a newer version is
compatible with the older ones when updating the script.
"""

from __future__ import print_function

import copy
import json
from optparse import OptionParser
import os
import re
import subprocess
import sys
import zipfile


def UnpackZip(target, source):
  """Unpacks |source| archive to |target| directory."""
  target = os.path.normpath(target)
  archive = zipfile.ZipFile(source, 'r')
  for f in archive.namelist():
    target_file = os.path.normpath(os.path.join(target, f))
    # Sanity check to make sure .zip uses relative paths.
    if os.path.commonprefix([target_file, target]) != target:
      print("Failed to unpack '%s': '%s' is not under '%s'" % (
          source, target_file, target))
      return 1

    # Create intermediate directories.
    target_dir = os.path.dirname(target_file)
    if not os.path.exists(target_dir):
      os.makedirs(target_dir)

    archive.extract(f, target)
  return 0


def Merge(left, right):
  """Merges two values.

  Raises:
    TypeError: |left| and |right| cannot be merged.

  Returns:
    - if both |left| and |right| are dictionaries, they are merged recursively.
    - if both |left| and |right| are lists, the result is a list containing
        elements from both lists.
    - if both |left| and |right| are simple value, |right| is returned.
    - |TypeError| exception is raised if a dictionary or a list are merged with
        a non-dictionary or non-list correspondingly.
  """
  if isinstance(left, dict):
    if isinstance(right, dict):
      retval = copy.copy(left)
      for key, value in right.items():
        if key in retval:
          retval[key] = Merge(retval[key], value)
        else:
          retval[key] = value
      return retval
    else:
      raise TypeError('Error: merging a dictionary and non-dictionary value')
  elif isinstance(left, list):
    if isinstance(right, list):
      return left + right
    else:
      raise TypeError('Error: merging a list and non-list value')
  else:
    if isinstance(right, dict):
      raise TypeError('Error: merging a dictionary and non-dictionary value')
    elif isinstance(right, list):
      raise TypeError('Error: merging a dictionary and non-dictionary value')
    else:
      return right

quote_matcher_regex = re.compile(r'\s|"')
quote_replacer_regex = re.compile(r'(\\*)"')


def QuoteArgument(arg):
  """Escapes a Windows command-line argument.

  So that the Win32 CommandLineToArgv function will turn the escaped result back
  into the original string.
  See http://msdn.microsoft.com/en-us/library/17w5ykft.aspx
  ("Parsing C++ Command-Line Arguments") to understand why we have to do
  this.

  Args:
      arg: the string to be escaped.
  Returns:
      the escaped string.
  """

  def _Replace(match):
    # For a literal quote, CommandLineToArgv requires an odd number of
    # backslashes preceding it, and it produces half as many literal backslashes
    # (rounded down). So we need to produce 2n+1 backslashes.
    return 2 * match.group(1) + '\\"'

  if re.search(quote_matcher_regex, arg):
    # Escape all quotes so that they are interpreted literally.
    arg = quote_replacer_regex.sub(_Replace, arg)
    # Now add unescaped quotes so that any whitespace is interpreted literally.
    return '"' + arg + '"'
  else:
    return arg


def GenerateCommandLine(tool, source, dest, parameters):
  """Generates the command line for |tool|."""
  # Merge/apply tool-specific parameters
  params = copy.copy(parameters)
  if tool in parameters:
    params = Merge(params, params[tool])

  wix_path = os.path.normpath(params.get('wix_path', ''))
  switches = [os.path.join(wix_path, tool), '-nologo']

  # Append the list of defines and extensions to the command line switches.
  for name, value in params.get('defines', {}).items():
    switches.append('-d%s=%s' % (name, value))

  for ext in params.get('extensions', []):
    switches += ('-ext', os.path.join(wix_path, ext))

  # Append raw switches
  switches += params.get('switches', [])

  # Append the input and output files
  switches += ('-out', dest, source)

  # Generate the actual command line
  #return ' '.join(map(QuoteArgument, switches))
  return switches


def Run(args):
  """Runs a command interpreting the passed |args| as a command line."""
  command = ' '.join(map(QuoteArgument, args))
  popen = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  out, _ = popen.communicate()
  if popen.returncode:
    print(command)
    for line in out.splitlines():
      print(line)
    print('%s returned %d' % (args[0], popen.returncode))
  return popen.returncode


def GenerateMsi(target, source, parameters):
  """Generates .msi from the installation files prepared by Chromium build."""
  parameters['basename'] = os.path.splitext(os.path.basename(source))[0]

  # The script can handle both forms of input a directory with unpacked files or
  # a ZIP archive with the same files. In the latter case the archive should be
  # unpacked to the intermediate directory.
  source_dir = None
  if os.path.isdir(source):
    # Just use unpacked files from the supplied directory.
    source_dir = source
  else:
    # Unpack .zip
    rc = UnpackZip(parameters['intermediate_dir'], source)
    if rc != 0:
      return rc
    source_dir = '%(intermediate_dir)s\\%(basename)s' % parameters

  # Read parameters from 'parameters.json'.
  f = open(os.path.join(source_dir, 'parameters.json'))
  parameters = Merge(json.load(f), parameters)
  f.close()

  if 'source' not in parameters:
    print('The source .wxs is not specified')
    return 1

  if 'bind_path' not in parameters:
    print('The binding path is not specified')
    return 1

  wxs = os.path.join(source_dir, parameters['source'])

  #  Add the binding path to the light-specific parameters.
  bind_path = os.path.join(source_dir, parameters['bind_path'])
  parameters = Merge(parameters, {'light': {'switches': ['-b', bind_path]}})

  target_arch = parameters['target_arch']
  if target_arch == 'ia32':
    arch_param = 'x86'
  elif target_arch == 'x64':
    arch_param = 'x64'
  else:
    print('Invalid target_arch parameter value')
    return 1

  # Add the architecture to candle-specific parameters.
  parameters = Merge(
      parameters, {'candle': {'switches': ['-arch', arch_param]}})

  # Run candle and light to generate the installation.
  wixobj = '%(intermediate_dir)s\\%(basename)s.wixobj' % parameters
  args = GenerateCommandLine('candle', wxs, wixobj, parameters)
  rc = Run(args)
  if rc:
    return rc

  args = GenerateCommandLine('light', wixobj, target, parameters)
  rc = Run(args)
  if rc:
    return rc

  return 0


def main():
  usage = 'Usage: zip2msi [options] <input.zip> <output.msi>'
  parser = OptionParser(usage=usage)
  parser.add_option('--intermediate_dir', dest='intermediate_dir', default='.')
  parser.add_option('--wix_path', dest='wix_path', default='.')
  parser.add_option('--target_arch', dest='target_arch', default='x86')
  options, args = parser.parse_args()
  if len(args) != 2:
    parser.error('two positional arguments expected')

  return GenerateMsi(args[1], args[0], dict(options.__dict__))

if __name__ == '__main__':
  sys.exit(main())


#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Disable the lint error for too-long lines for the URL below.
# pylint: disable=C0301

"""Fix Chrome App manifest.json files for use with multi-platform zip files.

See info about multi-platform zip files here:
https://developer.chrome.com/native-client/devguide/distributing#packaged-application

The manifest.json file needs to point to the correct platform-specific paths,
but we build all toolchains and configurations in the same tree. As a result,
we can't have one manifest.json for all combinations.

Instead, we update the top-level manifest.json file during the build:

  "platforms": [
    {
      "nacl_arch": "x86-64",
      "sub_package_path": "_platform_specific/x86-64/"
    },
    ...

Becomes

  "platforms": [
    {
      "nacl_arch": "x86-64",
      "sub_package_path": "<toolchain>/<config>/_platform_specific/x86-64/"
    },
    ...
"""

import argparse
import collections
import json
import os
import sys

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


class Error(Exception):
  """Local Error class for this file."""
  pass


def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')

Trace.verbose = False


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-p', '--prefix',
                      help='Prefix to set for all sub_package_paths in the '
                      'manifest. If none is specified, the prefix will be '
                      'removed; i.e. the start of the path will be '
                      '"_platform_specific/..."')
  parser.add_argument('-v', '--verbose',
                      help='Verbose output', action='store_true')
  parser.add_argument('manifest_json')

  options = parser.parse_args(args)
  if options.verbose:
    Trace.verbose = True

  Trace('Reading %s' % options.manifest_json)
  with open(options.manifest_json) as f:
    # Keep the dictionary order. This is only supported on Python 2.7+
    if sys.version_info >= (2, 7, 0):
      data = json.load(f, object_pairs_hook=collections.OrderedDict)
    else:
      data = json.load(f)

  if 'platforms' not in data:
    raise Error('%s does not have "platforms" key.' % options.manifest_json)

  platforms = data['platforms']
  if not isinstance(platforms, list):
    raise Error('Expected "platforms" key to be array.')

  if options.prefix:
    prefix = options.prefix + '/'
  else:
    prefix = ''

  for platform in platforms:
    nacl_arch = platform.get('nacl_arch')

    if 'sub_package_path' not in platform:
      raise Error('Expected each platform to have "sub_package_path" key.')

    sub_package_path = platform['sub_package_path']
    index = sub_package_path.find('_platform_specific')
    if index == -1:
      raise Error('Could not find "_platform_specific" in the '
                  '"sub_package_path" key.')

    new_path = prefix + sub_package_path[index:]
    platform['sub_package_path'] = new_path

    Trace('  %s: "%s" -> "%s"' % (nacl_arch, sub_package_path, new_path))

  with open(options.manifest_json, 'w') as f:
    json.dump(data, f, indent=2)

  return 0


if __name__ == '__main__':
  try:
    rtn = main(sys.argv[1:])
  except Error, e:
    sys.stderr.write('%s: %s\n' % (os.path.basename(__file__), e))
    rtn = 1
  except KeyboardInterrupt:
    sys.stderr.write('%s: interrupted\n' % os.path.basename(__file__))
    rtn = 1
  sys.exit(rtn)

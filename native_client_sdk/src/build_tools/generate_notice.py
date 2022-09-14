#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build the NOTICE file distributed with the NaCl SDK from a set of given
license files."""

import argparse
import os
import sys


def Trace(msg):
  if Trace.verbose:
    print msg

Trace.verbose = False


def FindFiles(files):
  found = [f for f in files if os.path.exists(f)]

  if Trace.verbose:
    for f in sorted(set(files) - set(found)):
      Trace('Skipping %s. File doesn\'t exist.\n' % (f,))

  return found


def CreateLicenseDict(files):
  # Many of the license files are duplicates. Create a map of license text to
  # filename.
  license_dict = {}
  for filename in files:
    license_text = open(filename).read()
    license_text = license_text.replace('\r\n', '\n')
    license_dict.setdefault(license_text, []).append(filename)

  # Flip the dictionary (map tuple of filenames -> license text).
  return dict((tuple(value), key) for key, value in license_dict.iteritems())


def WriteLicense(output_file, root, license_text, license_filenames):
  Trace('Writing license for files:\n' + '\n'.join(license_filenames))
  output_file.write('=' * 70 + '\n')
  for filename in sorted(license_filenames):
    filename = os.path.relpath(filename, root)
    license_dir = os.path.dirname(filename)
    if not license_dir:
      license_dir = 'native_client_sdk'

    output_file.write('%s is licensed as follows\n' % (license_dir,))
    output_file.write('  (Cf. %s):\n' % (filename,))
  output_file.write('=' * 70 + '\n')
  output_file.write(license_text)


def Generate(output_filename, root, files):
  found_files = FindFiles(files)
  license_dict = CreateLicenseDict(found_files)
  with open(output_filename, 'w') as output_file:
    for i, license_filenames in enumerate(sorted(license_dict.iterkeys())):
      license_text = license_dict[license_filenames]
      if i:
        output_file.write('\n\n\n')
      WriteLicense(output_file, root, license_text, license_filenames)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-v', '--verbose', help='Verbose output.',
      action='store_true')
  parser.add_argument('-o', '--output', help='Output file')
  parser.add_argument('--root', help='Root for all paths')
  parser.add_argument('files', nargs='*')

  options = parser.parse_args(args)
  Trace.verbose = options.verbose

  if not options.output:
    parser.error('No output file given. See -o.')
  if not options.root:
    parser.error('No root directory given. See --root.')

  Generate(options.output, options.root, options.files)
  Trace('Done.')

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

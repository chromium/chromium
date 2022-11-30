#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script to find a recently-built Chrome, in the likely places.
This script is used for automated testing, don't trust it for anything more
than that!"""


import optparse
import os
import sys


def FindChrome(src_dir, configs, verbose=False):
  # List of places that chrome could live.
  # In theory we should be more careful about what platform we're actually
  # building for.
  # As currently constructed, this will also hork people who have debug and
  # release builds sitting side by side who build locally.
  chrome_locations = []

  for config in configs:
    chrome_locations.extend([
        'build/%s/chrome.exe' % config,
        'chrome/%s/chrome.exe' % config,
        # Windows Chromium ninja builder
        'out/%s/chrome.exe' % config,
        # Linux
        'out/%s/chrome' % config,
        # Mac Chromium ninja builder
        'out/%s/Chromium.app/Contents/MacOS/Chromium' % config,
        # Mac release ninja builder
        'out/%s/Google Chrome.app/Contents/MacOS/Google Chrome' % config,
        # Mac Chromium xcode builder
        'xcodebuild/%s/Chromium.app/Contents/MacOS/Chromium' % config,
        # Mac release xcode builder
        'xcodebuild/%s/Google Chrome.app/Contents/MacOS/Google Chrome' % config,
    ])

  # Pick the one with the newest timestamp.
  latest_mtime = 0
  latest_path = None
  for chrome in chrome_locations:
    chrome_filename = os.path.join(src_dir, chrome)
    if verbose:
      print 'Looking for %r...' % chrome_filename,
    if os.path.exists(chrome_filename):
      if verbose:
        print 'YES.'
      mtime = os.path.getmtime(chrome_filename)
      if mtime > latest_mtime:
        latest_mtime = mtime
        latest_path = chrome_filename
    else:
      if verbose:
        print 'NO.'
  if latest_path is not None:
    if verbose:
      print 'Most recent is %r.' % latest_path
    return latest_path
  return None


def main(args):
  usage = 'Usage: %prog [options] <src dir>'
  description = __doc__
  parser = optparse.OptionParser(usage, description=description)
  parser.add_option('-c', '--config',
                    action='append',
                    help='Which configuration of Chrome to look for. '
                         'One of [Debug, Release]. The default is to try both. '
                         'You can specify this multiple times.')
  parser.add_option('-v', '--verbose', action='store_true',
                    help='Verbose output')

  options, args = parser.parse_args(args[1:])

  if not len(args):
    parser.error('Expected source directory as first argument.')

  if not options.config:
    options.config = ['Debug', 'Release']

  invalid_configs = set(options.config) - set(['Debug', 'Release'])
  if invalid_configs:
    parser.error('Expected config to be one of [Debug, Release]. '
                 'Got the following invalid configs: %s. ' %
                 ', '.invalid_configs)

  src_dir = args[0]
  chrome_path = FindChrome(src_dir, options.config, options.verbose)
  if not chrome_path:
    sys.stderr.write('Error: Cannot find Chrome. '
                     'Run again with -v to see where was searched.\n')
    return 1

  print chrome_path
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))

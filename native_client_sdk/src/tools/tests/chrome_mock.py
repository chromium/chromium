#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mock chrome process used by test code for http server."""

import argparse
import sys
import time
import urllib2

def PrintAndFlush(s):
  sys.stdout.write(s + '\n')
  sys.stdout.flush()

def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--post', help='POST to URL.', dest='post',
                      action='store_true')
  parser.add_argument('--get', help='GET to URL.', dest='get',
                      action='store_true')
  parser.add_argument('--sleep',
                      help='Number of seconds to sleep after reading URL',
                      dest='sleep', default=0)
  parser.add_argument('--expect-to-be-killed', help='If set, the script will'
                      ' warn if it isn\'t killed before it finishes sleeping.',
                      dest='expect_to_be_killed', action='store_true')
  parser.add_argument('url')

  options = parser.parse_args(args)

  PrintAndFlush('Starting %s.' % sys.argv[0])

  if options.post:
    urllib2.urlopen(options.url, data='').read()
  elif options.get:
    urllib2.urlopen(options.url).read()
  else:
    # Do nothing but wait to be killed.
    pass

  time.sleep(float(options.sleep))

  if options.expect_to_be_killed:
    PrintAndFlush('Done sleeping. Expected to be killed.')
  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

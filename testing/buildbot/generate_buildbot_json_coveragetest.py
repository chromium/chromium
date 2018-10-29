#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import coverage
import cStringIO
import sys
import unittest

class FakeStream(object):
  def write(self, value):
    pass

  def flush(self):
    pass

def main():
  cov = coverage.coverage(include='*generate_buildbot_json.py')
  cov.start()
  import generate_buildbot_json_unittest
  suite = unittest.TestLoader().loadTestsFromModule(
    generate_buildbot_json_unittest)
  unittest.TextTestRunner(stream=FakeStream()).run(suite)
  cov.stop()
  outf = cStringIO.StringIO()
  percentage = cov.report(file=outf, show_missing=True)
  if int(percentage) != 100:
    print(outf.getvalue())
    print('FATAL: Insufficient coverage (%.f%%)' % int(percentage))
    return 1
  return 0

if __name__ == '__main__':
  sys.exit(main())

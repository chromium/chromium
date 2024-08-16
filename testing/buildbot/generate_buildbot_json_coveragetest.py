#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import sys
import unittest

import coverage


class FakeStream(object):  # pylint: disable=useless-object-inheritance
  def write(self, value):
    pass

  def flush(self):
    pass

def main():
  cov = coverage.coverage(include='*generate_buildbot_json.py')
  cov.start()
  # pylint: disable=import-outside-toplevel
  import generate_buildbot_json_unittest
  # pylint: enable=import-outside-toplevel
  suite = unittest.TestLoader().loadTestsFromModule(
    generate_buildbot_json_unittest)
  unittest.TextTestRunner(stream=FakeStream()).run(suite)
  cov.stop()
  outf = io.StringIO()
  percentage = cov.report(file=outf, show_missing=True)
  if int(percentage) != 100:
    print(outf.getvalue())
    print('FATAL: Insufficient coverage (%.f%%)' % int(percentage))
    return 1
  return 0

if __name__ == '__main__':
  sys.exit(main())

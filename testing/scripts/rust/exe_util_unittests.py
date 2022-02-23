#!/usr/bin/env vpython3

# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
from pyfakefs import fake_filesystem_unittest
import sys
import tempfile
import unittest

import exe_util


class ExeUtilTests(fake_filesystem_unittest.TestCase):
    def test_run_and_tee_output(self):
        # Test wrapping Python as it echos a '.' character back.
        args = [sys.executable, '-c', 'print(\'.\')']
        output = exe_util.run_and_tee_output(args)
        self.assertEqual('.', output.strip())

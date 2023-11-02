#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""//testing/scripts wrapper for the network traffic annotations checks.
This script is used to run check_annotations.py on the trybots to ensure that
all network traffic annotations have correct syntax and semantics, and all
functions requiring annotations have one.

This is a wrapper around tools/traffic_annotation/scripts/auditor.py.

See tools/traffic_annotation/scripts/auditor/README.md for instructions on
running locally."""

import json
import os
import sys
import tempfile

# Add src/testing/ into sys.path for importing common without pylint errors.
sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.path.pardir)))
from scripts import common


def main_run(args):
  errors_file = tempfile.NamedTemporaryFile()
  errors_filename = errors_file.name
  errors_file.close()

  command_line = [
      sys.executable,
      os.path.join(common.SRC_DIR, 'tools', 'traffic_annotation', 'scripts',
                   'check_annotations.py'),
      '--build-path',
      os.path.join(args.paths['checkout'], 'out', args.build_config_fs),
      '--errors-file',
      errors_filename,
  ]
  rc = common.run_command(command_line)

  failures = []
  if rc:
    with open(errors_filename, encoding='utf-8') as f:
      failures = json.load(f) or ['Please refer to stdout for errors.']
  common.record_local_script_results(
      'check_network_annotations', args.output, failures, True)

  return rc


def main_compile_targets(args):
  json.dump(['traffic_annotation_auditor_dependencies'], args.output)


if __name__ == '__main__':
  funcs = {
    'run': main_run,
    'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))

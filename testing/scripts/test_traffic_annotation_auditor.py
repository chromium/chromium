#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""//testing/scripts wrapper for the network traffic annotation auditor checks.
This script is used to run traffic_annotation_auditor_tests.py on an FYI bot to
check that traffic_annotation_auditor has the same results when heuristics that
help it run fast and spam free on trybots are disabled."""

import json
import os
import sys
import tempfile


import common

SHEET_CONFIG = {
  "spreadsheet_id": "1TmBr9jnf1-hrjntiVBzT9EtkINGrtoBYFMWad2MBeaY",
  "annotations_sheet_name": "Annotations",
  "changes_sheet_name": "Changes Stats",
  "silent_change_columns": [],
  "last_update_column_name": "Last Update",
}


def is_windows():
  return os.name == 'nt'


def main_run(args):
  annotations_file = tempfile.NamedTemporaryFile()
  annotations_filename = annotations_file.name
  annotations_file.close()

  command_line = [
      sys.executable,
      os.path.join(common.SRC_DIR, 'tools', 'traffic_annotation', 'scripts',
                   'traffic_annotation_auditor_tests.py'),
      '--build-path',
      os.path.join(args.paths['checkout'], 'out', args.build_config_fs),
      '--annotations-file',
      annotations_filename,
  ]
  rc = common.run_command(command_line)

  # Update the Google Sheets on success, but only on the Windows trybot.
  if rc == 0 and is_windows():
    print("Tests succeeded. Updating annotations sheet...")

    config_file = tempfile.NamedTemporaryFile(delete=False)
    json.dump(SHEET_CONFIG, config_file, indent=4)
    config_filename = config_file.name
    config_file.close()

    command_line = [
      'vpython.bat',
      os.path.join(common.SRC_DIR, 'tools', 'traffic_annotation', 'scripts',
                   'update_annotations_sheet.py'),
      '--force',
      '--config-file',
      config_filename,
      '--annotations-file',
      annotations_filename,
    ]
    rc = common.run_command(command_line)

    try:
      os.remove(config_filename)
    except OSError:
      pass

  try:
    os.remove(annotations_filename)
  except OSError:
    pass

  json.dump({
      'valid': True,
      'failures': ['Please refer to stdout for errors.'] if rc else [],
  }, args.output)

  return rc


def main_compile_targets(args):
  json.dump(['all'], args.output)


if __name__ == '__main__':
  funcs = {
    'run': main_run,
    'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))

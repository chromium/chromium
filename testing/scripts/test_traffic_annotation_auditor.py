#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""//testing/scripts wrapper for the network traffic annotation auditor checks.
This script is used to run traffic_annotation_auditor_tests.py on an FYI bot to
check that traffic_annotation_auditor has the same results when heuristics that
help it run fast and spam free on trybots are disabled."""

import json
import logging
import os
import re
import sys
import tempfile
import traceback

import common

WINDOWS_SHEET_CONFIG = {
    'spreadsheet_id': '1TmBr9jnf1-hrjntiVBzT9EtkINGrtoBYFMWad2MBeaY',
    'annotations_sheet_name': 'Annotations',
    'chrome_version_sheet_name': 'Chrome Version',
    'silent_change_columns': [],
    'last_update_column_name': 'Last Update',
}

CHROMEOS_SHEET_CONFIG = {
    'spreadsheet_id': '1928goWKy6LVdF9Nl5nV1OD260YC10dHsdrnHEGdGsg8',
    'annotations_sheet_name': 'Annotations',
    'chrome_version_sheet_name': 'Chrome Version',
    'silent_change_columns': [],
    'last_update_column_name': 'Last Update',
}


def is_windows():
  return os.name == 'nt'


def is_chromeos(build_path):
  current_platform = get_current_platform_from_gn_args(build_path)
  return current_platform == 'chromeos'


def get_sheet_config(build_path):
  if is_windows():
    return WINDOWS_SHEET_CONFIG
  if is_chromeos(build_path):
    return CHROMEOS_SHEET_CONFIG
  return None


def get_current_platform_from_gn_args(build_path):
  if sys.platform.startswith('linux') and build_path is not None:
    try:
      with open(os.path.join(build_path, 'args.gn')) as f:
        gn_args = f.read()
      if not gn_args:
        logging.info('Could not retrieve args.gn')

      pattern = re.compile(r'^\s*target_os\s*=\s*"chromeos"\s*$', re.MULTILINE)
      if pattern.search(gn_args):
        return 'chromeos'

    except (ValueError, OSError) as e:
      logging.info(e)

  return None


def main_run(args):
  annotations_file, annotations_filename = tempfile.mkstemp()
  os.close(annotations_file)

  errors_file, errors_filename = tempfile.mkstemp()
  os.close(errors_file)

  build_path = args.build_dir
  command_line = [
      sys.executable,
      os.path.join(common.SRC_DIR, 'tools', 'traffic_annotation', 'scripts',
                   'traffic_annotation_auditor_tests.py'),
      '--build-path',
      build_path,
      '--annotations-file',
      annotations_filename,
      '--errors-file',
      errors_filename,
  ]
  rc = common.run_command(command_line)

  # Update the Google Sheets on success, but only on the Windows and ChromeOS
  # trybot.
  update_sheet = '--no-update-sheet' not in args.args
  sheet_config = get_sheet_config(build_path)
  try:
    if rc:
      print('Test failed without updating the annotations sheet.')
      with open(errors_filename, encoding='utf-8') as f:
        failures = json.load(f) or ['Please refer to stdout for errors.']
    else:
      print('Tests succeeded.')
      failures = []

      if update_sheet and sheet_config is not None:
        print('Updating annotations sheet...')
        config_file = tempfile.NamedTemporaryFile(delete=False, mode='w+')
        json.dump(sheet_config, config_file, indent=4)
        config_filename = config_file.name
        config_file.close()
        vpython_path = 'vpython3.bat' if is_windows() else 'vpython3'

        command_line = [
            vpython_path,
            os.path.join(common.SRC_DIR, 'tools', 'traffic_annotation',
                         'scripts', 'update_annotations_sheet.py'),
            '--yes',
            '--config-file',
            config_filename,
            '--annotations-file',
            annotations_filename,
        ]
        rc = common.run_command(command_line)
        cleanup_file(config_filename)

        if rc:
          failures = ['Please refer to stdout for errors.']

    common.record_local_script_results('test_traffic_annotation_auditor',
                                       args.output, failures, True)
  except (ValueError, OSError) as e:
    print('Error updating the annotations sheet', e)
    traceback.print_exc()
  finally:
    cleanup_file(annotations_filename)

  return rc


def cleanup_file(filename):
  try:
    os.remove(filename)
  except OSError:
    print('Could not remove file: ', filename)


def main_compile_targets(args):
  json.dump(['traffic_annotation_proto'], args.output)


if __name__ == '__main__':
  funcs = {
      'run': main_run,
      'compile_targets': main_compile_targets,
  }
  sys.exit(common.run_script(sys.argv[1:], funcs))

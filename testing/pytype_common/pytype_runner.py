# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import time
import typing

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..'))

sys.path.append(os.path.join(CHROMIUM_SRC_DIR, 'build', 'util'))

from lib.results import result_sink
from lib.results import result_types


# pylint: disable=too-many-arguments
def report_results(test_name: str,
                   test_location: str,
                   status: str,
                   duration: float,
                   log: str,
                   output_file: typing.Optional[str],
                   sink_client: typing.Optional[result_sink.ResultSinkClient],
                   failure_reason: typing.Optional[str] = None) -> None:
    """Report results on bots.

    Args:
        test_name: The name of the test to report.
        test_location: The Chromium src-relative path (starting with //) of the
            test file that will be reported in results. Usually the path to
            whatever script is calling this function.
        status: A string containing the test status.
        duration: An float containing the test duration in seconds.
        log: A string containing the log output of the test.
        output_dir: An optional string containing a path to a file to output
            JSON to.
        sink_client: An optional client for reporting results to ResultDB.
        failure_reason: An optional string containing a reason why the test
            failed.
    """
    if output_file:
        report_json_results(output_file)
    if sink_client:
        sink_client.Post(test_id=test_name,
                         status=status,
                         duration=(duration * 1000),
                         test_log=log,
                         test_file=test_location,
                         failure_reason=failure_reason)


# pylint: enable=too-many-arguments


def report_json_results(output_file: str) -> None:
    """'Report' results on bots.

    Actually just writes an empty JSON object to a file since all we need to
    do is make the merge scripts happy.

    Args:
        output_dir: An optional string containing a path to a file to output
            JSON to.
    """
    with open(output_file, 'w') as outfile:
        json.dump({}, outfile)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument('--isolated-script-test-output',
                        dest='output_file',
                        help=('Path to JSON output file.'))

    args, _ = parser.parse_known_args()
    return args


def run_pytype(test_name: str, test_location: str,
               files_to_check: typing.Iterable[str],
               python_paths: typing.Iterable[str], cwd: str) -> int:
    """Runs pytype on a given list of files/directories.

    Args:
        test_name: The name of the test that will be reported in results.
        test_location: The Chromium src-relative path (starting with //) of the
            test file that will be reported in results. Usually the path to
            whatever script is calling this function.
        files_to_check: Files and directories to run pytype on as absolute
            paths.
        python_paths: Any paths that should be set as PYTHONPATH when running
            pytype.
        cwd: The directory that pytype should be run from.

    Returns:
        0 on success, non-zero on failure.
    """
    sink_client = result_sink.TryInitClient()
    args = parse_args()

    if sys.platform != 'linux':
        print('pytype is currently only supported on Linux, see '
              'https://github.com/google/pytype/issues/1154')
        report_results(test_name, test_location, result_types.SKIP, 0,
                       'Skipped due to unsupported platform.',
                       args.output_file, sink_client)
        return 0

    # Strangely, pytype won't complain if you tell it to analyze a directory
    # that
    # doesn't exist, which could potentially lead to code not being analyzed if
    # it's added here but not added to the isolate. So, ensure that everything
    # we expect to analyze actually exists.
    for f in files_to_check:
        if not os.path.exists(f):
            raise RuntimeError(
                'Requested file or directory %s does not exist.' % f)

    # pytype looks for a 'python' or 'python3' executable in PATH, so make sure
    # that the Python 3 executable from vpython is in the path.
    executable_dir = os.path.dirname(sys.executable)
    os.environ['PATH'] = executable_dir + os.pathsep + os.environ['PATH']

    # pytype specifies that the provided PYTHONPATH is :-separated.
    pythonpath = ':'.join(python_paths)
    pytype_cmd = [
        sys.executable,
        '-m',
        'pytype',
        '--pythonpath',
        pythonpath,
        '--keep-going',
        '--jobs',
        'auto',
    ]
    pytype_cmd.extend(files_to_check)

    if sink_client:
        stdout_handle = subprocess.PIPE
        stderr_handle = subprocess.STDOUT
    else:
        stdout_handle = None
        stderr_handle = None

    start_time = time.time()
    try:
        proc = subprocess.run(pytype_cmd,
                              check=True,
                              cwd=cwd,
                              stdout=stdout_handle,
                              stderr=stderr_handle,
                              text=True)
        stdout = proc.stdout
        status = result_types.PASS
        failure_reason = None
    except subprocess.CalledProcessError as e:
        stdout = e.stdout
        status = result_types.FAIL
        failure_reason = 'Checking Python 3 type hinting failed.'
    duration = (time.time() - start_time)

    if stdout:
        print(stdout)
    report_results(test_name, test_location, status, duration, stdout or '',
                   args.output_file, sink_client, failure_reason)

    if status == result_types.FAIL:
        return 1
    return 0

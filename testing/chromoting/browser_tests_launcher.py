# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Utility script to launch browser-tests on the Chromoting bot."""

from __future__ import print_function

import argparse
import time

from chromoting_test_utilities import CleanupUserProfileDir
from chromoting_test_utilities import GetJidFromHostLog
from chromoting_test_utilities import GetJidListFromTestResults
from chromoting_test_utilities import InitialiseTestMachineForLinux
from chromoting_test_utilities import MAX_RETRIES
from chromoting_test_utilities import PrintHostLogContents
from chromoting_test_utilities import PROD_DIR_ID
from chromoting_test_utilities import RunCommandInSubProcess
from chromoting_test_utilities import TestCaseSetup
from chromoting_test_utilities import TestMachineCleanup

SUCCESS_INDICATOR = 'SUCCESS: all tests passed.'
TEST_FAILURE = False
FAILING_TESTS = ''
BROWSER_NOT_STARTED_ERROR = (
    'Still waiting for the following processes to finish')
TIME_OUT_INDICATOR = '(TIMED OUT)'


def LaunchBTCommand(args, command):
  """Launches the specified browser-test command.

    Retry if the execution failed because a browser-instance was not launched or
    because the JID used did not match the host-JID.
  Args:
    args: Command line args, used for test-case startup tasks.
    command: Browser-test command line.

  Returns:
    host_log_file_names: Array of host logs created for this command, including
         retries.
  """
  global TEST_FAILURE, FAILING_TESTS
  host_log_file_names = []

  retries = 0
  host_jid_mismatch = False
  host_jid = None
  while retries <= MAX_RETRIES:
    # TestCaseSetup restarts the me2me host, and sets up user-profile dir.
    # It returns the file-name of the me2me host log.
    # If we are attempting to run this test because of a JID-mismatch, don't
    # restart host.
    if host_jid_mismatch:
      # Cleanup user-profile directory, but don't restart host.
      CleanupUserProfileDir(args)
    else:
      host_log_file_names.append(TestCaseSetup(args))
      # Parse the me2me host log to obtain the JID that the host registered.
      host_jid = GetJidFromHostLog(host_log_file_names[retries])

    results = RunCommandInSubProcess(command)

    # Get the JID used by this test to connect a remote-host, if any.
    jids_used = GetJidListFromTestResults(results)

    # Check for JID mismatch before checking for test success, so that we may
    # record instances where a test passed despite a JID mismatch.
    if jids_used and host_jid.rstrip() not in jids_used:
      host_jid_mismatch = True
      print('Host JID mismatch. JID in host log = %s.' % host_jid.rstrip())
      print('Host JIDs used by test:')
      for jid in jids_used:
        print(jid)

    if host_jid_mismatch:
      # The JID for the remote-host did not match the JID that was used for this
      # execution of the test. This happens because of a replication delay in
      # updating all instances of the Chromoting Directory Server. To
      # work-around this, sleep for 30s, which, based off a recent (08/2015)
      # query for average replication delay for Chromoting, should be sufficient
      # for the current JID value to have fully propagated.
      retries += 1
      time.sleep(30)
      continue
    elif jids_used:
      print('JID used by test matched me2me host JID: %s' % host_jid)
    else:
      # There wasn't a mismatch and no JIDs were returned. If no JIDs were
      # returned, that means the test didn't use any JIDs, so there is nothing
      # further for us to do.
      pass

    if SUCCESS_INDICATOR in results:
      break

    # Sometimes, during execution of browser-tests, a browser instance is
    # not started and the test times out. See http://crbug/480025.
    # To work around it, check if this execution failed owing to that
    # problem and retry.
    # There are 2 things to look for in the results:
    # A line saying "Still waiting for the following processes to finish",
    # and, because sometimes that line gets logged even if the test
    # eventually passes, we'll also look for "(TIMED OUT)", before retrying.
    if BROWSER_NOT_STARTED_ERROR in results and TIME_OUT_INDICATOR in results:
      print('Browser-instance not started (http://crbug/480025). Retrying.')
    else:
      print('Test failed for unknown reason. Retrying.')

    retries += 1

  # Check that the test passed.
  if SUCCESS_INDICATOR not in results:
    TEST_FAILURE = True
    # Add this command-line to list of tests that failed.
    FAILING_TESTS += command

  return host_log_file_names


def main(args):

  InitialiseTestMachineForLinux(args.cfg_file)

  host_log_files = []
  with open(args.commands_file) as f:
    for line in f:
      # Replace the PROD_DIR value in the command-line with
      # the passed in value.
      line = line.replace(PROD_DIR_ID, args.prod_dir)
      # Launch specified command line for test.
      host_log_files.extend(LaunchBTCommand(args, line))

  # All tests completed. Include host-logs in the test results.
  PrintHostLogContents(host_log_files)

  return host_log_files

if __name__ == '__main__':

  parser = argparse.ArgumentParser()
  parser.add_argument('-f', '--commands_file',
                      help='path to file listing commands to be launched.')
  parser.add_argument('-p', '--prod_dir',
                      help='path to folder having product and test binaries.')
  parser.add_argument('-c', '--cfg_file',
                      help='path to test host config file.')
  parser.add_argument('--me2me_manifest_file',
                      help='path to me2me host manifest file.')
  parser.add_argument('--it2me_manifest_file',
                      help='path to it2me host manifest file.')
  parser.add_argument(
      '-u', '--user_profile_dir',
      help='path to user-profile-dir, used by connect-to-host tests.')
  command_line_args = parser.parse_args()
  host_logs = ''
  try:
    host_logs = main(command_line_args)
    if TEST_FAILURE:
      print('++++++++++AT LEAST 1 TEST FAILED++++++++++')
      print(FAILING_TESTS.rstrip('\n'))
      print('++++++++++++++++++++++++++++++++++++++++++')
      raise Exception('At least one test failed.')
  finally:
    # Stop host and cleanup user-profile-dir.
    TestMachineCleanup(command_line_args.user_profile_dir, host_logs)

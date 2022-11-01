#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest.mock as mock
import sys
import unittest
import subprocess
import os
import signal

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import PLUGIN_PROTOS_PATH, MAX_RECORDED_COUNT
from test_plugins import VideoRecorderPlugin

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

TEST_DEVICE_ID = '123'
TEST_CASE_NAME = '[AAA_BBB]'
TEST_CASE_INFO = test_plugin_service_pb2.TestCaseInfo(name=TEST_CASE_NAME)
OUT_DIR = 'out/dir'


class VideoRecorderPluginTest(unittest.TestCase):

  @mock.patch("subprocess.Popen")
  def test_test_case_will_start_succeed(self, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    cmd = [
        'xcrun', 'simctl', 'io', TEST_DEVICE_ID, 'recordVideo', '--codec=h264',
        '-f', file_dir
    ]
    mock_popen.assert_called_once_with(cmd)
    self.assertTrue(video_recorder_plugin.recording_process.test_case_name ==
                    TEST_CASE_NAME)

  @mock.patch("subprocess.Popen")
  def test_test_case_will_start_exceedMaxRecordedCount(self, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.testcase_recorded_count[
        TEST_CASE_NAME] = MAX_RECORDED_COUNT
    video_recorder_plugin.test_case_will_start(request)
    mock_popen.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_will_start_previousProcessNotTerminated(
      self, mock_os_remove, mock_os_kill, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)
    video_recorder_plugin.test_case_will_start(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    cmd = [
        'xcrun', 'simctl', 'io', TEST_DEVICE_ID, 'recordVideo', '--codec=h264',
        '-f', file_dir
    ]
    mock_popen.assert_called_with(cmd)

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_fail_succeed(self, mock_os_remove, mock_os_kill,
                                      mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case fails
    request = test_plugin_service_pb2.TestCaseDidFailRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_did_fail(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGINT)
    mock_os_remove.assert_not_called()
    self.assertTrue(video_recorder_plugin.recording_process.process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process.test_case_name == None)
    self.assertTrue(
        video_recorder_plugin.testcase_recorded_count[TEST_CASE_NAME] == 1)

  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_fail_noRecordingRunning(self, mock_os_remove,
                                                 mock_os_kill):
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseDidFailRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_did_fail(request)
    mock_os_kill.assert_not_called()
    mock_os_remove.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_succeed(self, mock_os_remove, mock_os_kill,
                                        mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case finishes
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(video_recorder_plugin.recording_process.process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process.test_case_name == None)
    self.assertTrue(
        TEST_CASE_NAME not in video_recorder_plugin.testcase_recorded_count)

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_remove_file_failed(self, mock_os_remove,
                                                   mock_os_kill, mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case finishes
    mock_os_remove.side_effect = FileNotFoundError
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO)
    # this should not throw exception because it's caught
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(video_recorder_plugin.recording_process.process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process.test_case_name == None)
    self.assertTrue(
        TEST_CASE_NAME not in video_recorder_plugin.testcase_recorded_count)

  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_noRecordingRunning(self, mock_os_remove,
                                                   mock_os_kill):
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_not_called()
    mock_os_remove.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_reset_succeed(self, mock_os_remove, mock_os_kill, mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(TEST_DEVICE_ID, OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # reset
    video_recorder_plugin.reset()
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(video_recorder_plugin.recording_process.process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process.test_case_name == None)

    # reset again to make sure no exception is thrown
    video_recorder_plugin.reset()


if __name__ == '__main__':
  unittest.main()

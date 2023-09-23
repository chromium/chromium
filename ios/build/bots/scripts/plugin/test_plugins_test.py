#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest.mock as mock
import sys
import unittest
import subprocess
import os
import signal
import glob
import copy

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))

# if executing from plugin directory, pull in scripts
else:
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), '..'))
from plugin_constants import PLUGIN_PROTOS_PATH, MAX_RECORDED_COUNT
from test_plugins import VideoRecorderPlugin, BasePlugin, FileCopyPlugin
import iossim_util

sys.path.append(PLUGIN_PROTOS_PATH)
import test_plugin_service_pb2
import test_plugin_service_pb2_grpc

TEST_DEVICE_ID = '123'
TEST_DEVICE_NAME = 'simulator_x_y'
TEST_DEVICE_PATH = '/root/dir'
TEST_CASE_NAME = '[AAA_BBB]'
TEST_CASE_INFO = test_plugin_service_pb2.TestCaseInfo(name=TEST_CASE_NAME)
TEST_DEVICE_INFO = test_plugin_service_pb2.DeviceInfo(name=TEST_DEVICE_NAME)
OUT_DIR = 'out/dir'
TEST_DEVICE_CACHE = {
    TEST_DEVICE_NAME: {
        'UDID': TEST_DEVICE_ID,
        'path': TEST_DEVICE_PATH
    }
}


class BasePluginTest(unittest.TestCase):

  @mock.patch("iossim_util.get_simulator_list")
  def test_get_udid_and_path_for_device_name_no_cache(self, mock_get_list):
    mock_get_list.return_value = {
        'devices': {
            'RUNTIME': [{
                'name': TEST_DEVICE_NAME,
                'udid': TEST_DEVICE_ID
            }]
        }
    }
    cache = {}
    base_plugin = BasePlugin(cache, 'OUT_DIR')

    self.assertEqual(
        base_plugin.get_udid_and_path_for_device_name(TEST_DEVICE_NAME,
                                                      [TEST_DEVICE_PATH]),
        (TEST_DEVICE_ID, TEST_DEVICE_PATH))
    mock_get_list.assert_called_once_with(TEST_DEVICE_PATH)
    self.assertEqual(
        base_plugin.device_info_cache.get(TEST_DEVICE_NAME), {
            'UDID': TEST_DEVICE_ID,
            'path': TEST_DEVICE_PATH,
        })

  @mock.patch('iossim_util.get_simulator_list')
  def test_get_udid_and_path_for_device_name_with_cache(self, mock_get_list):
    base_plugin = BasePlugin(TEST_DEVICE_CACHE, 'OUT_DIR')

    self.assertEqual(
        base_plugin.get_udid_and_path_for_device_name(TEST_DEVICE_NAME),
        (TEST_DEVICE_ID, TEST_DEVICE_PATH))
    mock_get_list.assert_not_called()


class VideoRecorderPluginTest(unittest.TestCase):

  @mock.patch("subprocess.Popen")
  def test_test_case_will_start_succeed(self, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    cmd = [
        'xcrun', 'simctl', '--set', TEST_DEVICE_PATH, 'io', TEST_DEVICE_ID,
        'recordVideo', '--codec=h264', '-f', file_dir
    ]
    mock_popen.assert_called_once_with(cmd)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).test_case_name == TEST_CASE_NAME)

  @mock.patch("subprocess.Popen")
  def test_test_case_will_start_exceedMaxRecordedCount(self, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.testcase_recorded_count[
        TEST_CASE_NAME] = MAX_RECORDED_COUNT
    video_recorder_plugin.test_case_will_start(request)
    mock_popen.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_will_start_previousProcessNotTerminated(
      self, mock_os_remove, mock_os_kill, mock_popen):
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)
    video_recorder_plugin.test_case_will_start(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    cmd = [
        'xcrun', 'simctl', '--set', TEST_DEVICE_PATH, 'io', TEST_DEVICE_ID,
        'recordVideo', '--codec=h264', '-f', file_dir
    ]
    mock_popen.assert_called_with(cmd)

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_fail_succeed(self, mock_os_remove, mock_os_kill,
                                      mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case fails
    request = test_plugin_service_pb2.TestCaseDidFailRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_did_fail(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGINT)
    mock_os_remove.assert_not_called()
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).test_case_name == None)
    self.assertTrue(
        video_recorder_plugin.testcase_recorded_count[TEST_CASE_NAME] == 1)

  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_fail_noRecordingRunning(self, mock_os_remove,
                                                 mock_os_kill):
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseDidFailRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_did_fail(request)
    mock_os_kill.assert_not_called()
    mock_os_remove.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_succeed(self, mock_os_remove, mock_os_kill,
                                        mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case finishes
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).test_case_name == None)
    self.assertTrue(
        TEST_CASE_NAME not in video_recorder_plugin.testcase_recorded_count)

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_remove_file_failed(self, mock_os_remove,
                                                   mock_os_kill, mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # then test case finishes
    mock_os_remove.side_effect = FileNotFoundError
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    # this should not throw exception because it's caught
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).test_case_name == None)
    self.assertTrue(
        TEST_CASE_NAME not in video_recorder_plugin.testcase_recorded_count)

  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_test_case_did_finish_noRecordingRunning(self, mock_os_remove,
                                                   mock_os_kill):
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseDidFinishRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_did_finish(request)
    mock_os_kill.assert_not_called()
    mock_os_remove.assert_not_called()

  @mock.patch("subprocess.Popen")
  @mock.patch("os.kill")
  @mock.patch("os.remove")
  def test_reset_succeed(self, mock_os_remove, mock_os_kill, mock_popen):
    # first, start recording
    video_recorder_plugin = VideoRecorderPlugin(
        copy.deepcopy(TEST_DEVICE_CACHE), OUT_DIR)
    request = test_plugin_service_pb2.TestCaseWillStartRequest(
        test_case_info=TEST_CASE_INFO, device_info=TEST_DEVICE_INFO)
    video_recorder_plugin.test_case_will_start(request)

    # reset
    video_recorder_plugin.reset()
    mock_os_kill.assert_called_once_with(mock.ANY, signal.SIGTERM)
    file_name = video_recorder_plugin.get_video_file_name(TEST_CASE_NAME, 0)
    file_dir = os.path.join(OUT_DIR, file_name)
    mock_os_remove.assert_called_once_with(file_dir)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).process == None)
    self.assertTrue(
        video_recorder_plugin.recording_process_for_device_name(
            TEST_DEVICE_NAME).test_case_name == None)

    # reset again to make sure no exception is thrown
    video_recorder_plugin.reset()


class FileCopyPluginTest(unittest.TestCase):

  @mock.patch("os.path.exists")
  @mock.patch("os.mkdir")
  @mock.patch("glob.glob")
  @mock.patch("shutil.move")
  def testOutputPathExists(self, move_mock: mock.MagicMock,
                           glob_mock: mock.MagicMock,
                           mkdir_mock: mock.MagicMock,
                           path_mock: mock.MagicMock):
    path_mock.return_value = True
    glob_mock.return_value = ["glob_return_value"]

    file_copy_plugin = FileCopyPlugin('GLOB_PATTERN', OUT_DIR,
                                      copy.deepcopy(TEST_DEVICE_CACHE))
    request = test_plugin_service_pb2.TestBundleWillFinishRequest(
        device_info=TEST_DEVICE_INFO)

    file_copy_plugin.test_bundle_will_finish(request)

    mkdir_mock.assert_not_called()
    path_mock.assert_called_once_with(OUT_DIR)
    glob_mock.assert_called_once_with(
        os.path.join(TEST_DEVICE_PATH, TEST_DEVICE_ID, "GLOB_PATTERN"))
    move_mock.assert_called_once_with("glob_return_value", OUT_DIR)

  @mock.patch("os.path.exists")
  @mock.patch("os.mkdir")
  @mock.patch("glob.glob")
  @mock.patch("shutil.move")
  def testOutputPathDoesNotExist(self, move_mock: mock.MagicMock,
                                 glob_mock: mock.MagicMock,
                                 mkdir_mock: mock.MagicMock,
                                 path_mock: mock.MagicMock):
    path_mock.return_value = False
    glob_mock.return_value = ["glob_return_value"]

    file_copy_plugin = FileCopyPlugin('GLOB_PATTERN', OUT_DIR,
                                      copy.deepcopy(TEST_DEVICE_CACHE))
    request = test_plugin_service_pb2.TestBundleWillFinishRequest(
        device_info=TEST_DEVICE_INFO)

    file_copy_plugin.test_bundle_will_finish(request)

    mkdir_mock.assert_called_once_with(OUT_DIR)
    path_mock.assert_called_once_with(OUT_DIR)
    glob_mock.assert_called_once_with(
        os.path.join(TEST_DEVICE_PATH, TEST_DEVICE_ID, "GLOB_PATTERN"))
    move_mock.assert_called_once_with("glob_return_value", OUT_DIR)


if __name__ == '__main__':
  unittest.main()

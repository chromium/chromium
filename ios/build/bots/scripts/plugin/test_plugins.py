# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import signal

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_constants import MAX_RECORDED_COUNT

LOGGER = logging.getLogger(__name__)


class BasePlugin(object):
  """ Base plugin class """

  def __init__(self, device_id, out_dir):
    """ Initializes a new instance of this class.

    Args:
      device_id: device id of the tests we are running on, useful for
      invoking xc commands target that specific device.
      out_dir: output directory for saving any useful data

    """
    self.device_id = device_id
    self.out_dir = out_dir

  def test_case_will_start(self, request):
    """ Required method to implement when a test case is about to start """
    raise NotImplementedError("test_case_will_start method not defined")

  def test_case_did_finish(self, request):
    """ Required method to implement when a test case is finished executing

    Note that this method will always be called at the end of a test case
    execution, regardless whether a test case failed or not.
    """
    raise NotImplementedError("test_case_did_finish method not defined")

  def test_case_did_fail(self, request):
    """ Required method to implement when a test case failed unexpectedly

    Note that this method is being called right before test_case_did_finish,
    if the test case failed unexpectedly.
    """
    raise NotImplementedError("test_case_did_fail method not defined")

  def reset(self):
    """
    Required method to implement to reset any running process/state
    in between each test attempt
    """
    raise NotImplementedError("reset method not defined")

  def start_proc(self, cmd):
    """ Starts a non-block process

    Args:
      cmd: the shell command to be executed

    """
    LOGGER.info('Executing command: %s', cmd)
    return subprocess.Popen(cmd)


class VideoRecorderPlugin(BasePlugin):
  """ Video plugin class for recording test execution """

  def __init__(self, device_id, out_dir):
    """ Initializes a new instance of this class, which is a subclass
    of BasePlugin

    Args:
      device_id: device id of the tests we are running on, useful for
      invoking xc commands target that specific device.
      out_dir: output directory where the video plugin should be saved to

    """
    super(VideoRecorderPlugin, self).__init__(device_id, out_dir)

    self.testcase_recorded_count = {}
    self.recording_process = RecordingProcess()

  def __str__(self):
    return "VideoRecorderPlugin"

  def test_case_will_start(self, request):
    """ Executes when a test class is about to start...

    This method will run a shell command to start video recording on
    the simulator. However, if a test case has been recorded for more
    than the maximum amount of times, then it will do nothing because
    there's no point in recording the same video over and over again
    and occupies disk space.
    Furthermore, there should be only one video recording process
    running at any given time. If the previous video recording process
    was not terminated for some reason (ideally it should), it will
    kill the existing process and starts a new process
    """
    LOGGER.info('Starting to record video for test case %s',
                request.test_case_info.name)
    attempt_count = self.testcase_recorded_count.get(
        request.test_case_info.name, 0)
    if (attempt_count >= MAX_RECORDED_COUNT):
      LOGGER.info('%s has been recorded for at least %s times, skipping...',
                  request.test_case_info.name, MAX_RECORDED_COUNT)
      return
    if (self.recording_process.process != None):
      LOGGER.warning(
          'Previous recording for test case %s is still ongoing, '
          'terminating before starting new recording...',
          self.recording_process.test_case_name)
      self.stop_recording(False)

    file_name = self.get_video_file_name(request.test_case_info.name,
                                         attempt_count)
    file_dir = os.path.join(self.out_dir, file_name)
    cmd = [
        'xcrun', 'simctl', 'io', self.device_id, 'recordVideo', '--codec=h264',
        '-f', file_dir
    ]
    process = self.start_proc(cmd)
    self.recording_process.process = process
    self.recording_process.test_case_name = request.test_case_info.name

  def test_case_did_fail(self, request):
    """ Executes when a test class fails unexpectedly...

    This method will terminate the existing running video recording process
    iff the test name in the request matches the existing running process's
    test name.
    It will also save the video file to local disk (by default).
    Otherwise, it will do nothing.
    """
    if (request.test_case_info.name == self.recording_process.test_case_name):
      self.stop_recording(True)
      self.testcase_recorded_count[request.test_case_info.name] = (
          self.testcase_recorded_count.get(request.test_case_info.name, 0) + 1)
    else:
      LOGGER.warning('No video recording process is currently running for %s',
                     request.test_case_info.name)

  def test_case_did_finish(self, request):
    """ Executes when a test class finishes executing...

    This method will terminate the existing running video recording process
    iff the test name in the request matches the existing running process's
    test name.
    It will not save the video file to local disk.
    Otherwise, it will do nothing.
    """
    if (request.test_case_info.name == self.recording_process.test_case_name):
      self.stop_recording(False)
      self.recording_process.reset()
    else:
      LOGGER.warning('No video recording process is currently running for %s',
                     request.test_case_info.name)

  def stop_recording(self, should_save):
    """ Terminate existing running video recording process

    Args:
      shouldSave: required flag to decide whether the recorded vide should
      be saved to local disk.

    """
    LOGGER.info('Terminating video recording process for test case %s',
                self.recording_process.test_case_name)
    if not should_save:
      # SIGTERM will immediately terminate the process, and the video
      # file will be left corrupted. We will still need to delete the
      # corrupted video file.
      os.kill(self.recording_process.process.pid, signal.SIGTERM)
      attempt_count = self.testcase_recorded_count.get(
          self.recording_process.test_case_name, 0)
      file_name = self.get_video_file_name(
          self.recording_process.test_case_name, attempt_count)
      file_dir = os.path.join(self.out_dir, file_name)
      LOGGER.info('shouldSave is false, deleting video file %s', file_dir)
      try:
        # Sometimes the video file is deleted together with the SIGTERM
        # signal, so we encounter FileNotFound error when trying to remove
        # the file again. We should catch any exception when removing files
        # because it shouldn't block future tests from being run.
        os.remove(file_dir)
      except Exception as e:
        LOGGER.warning('Failed to delete video file with error %s', e)
    else:
      # SIGINT will send a signal to terminate the process, and the video
      # will be written to the file asynchronously, while the process is
      # being terminated gracefully
      os.kill(self.recording_process.process.pid, signal.SIGINT)

    self.recording_process.reset()

  def reset(self):
    """ Executes in between each test attempet to reset any running
    process/state.

    This method will stop existing running video recording process,
    if there is any.
    """
    LOGGER.info('Clearing any running processes...')
    if (self.recording_process.process != None):
      self.stop_recording(False)

  def get_video_file_name(self, test_case_name, attempt_count):
    # Remove all non-word characters (everything except numbers and letters)
    s = re.sub(r"[^\w\s]", '', test_case_name)
    # replace all whitespace with underscore
    s = re.sub(r"\s+", '_', s)
    # add attempt num at the beginning, Video at the end
    s = 'attempt_' + str(attempt_count) + '_' + s + '_Video.mov'
    return s


class RecordingProcess:
  """
  Class for storing any useful data for existing running
  video recording process
  """

  def __init__(self):
    """ Initially, there should be no process and test case running """
    self.process = None
    self.test_case_name = None

  def reset(self):
    """ Resets all the info to None """
    self.process = None
    self.test_case_name = None

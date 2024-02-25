# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import logging
import os
import re
import shutil
import subprocess
import signal
import sys
from typing import List, Tuple, Optional

# if the current directory is in scripts (pwd), then we need to
# add plugin in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))

# if executing from plugin directory, pull in scripts
else:
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), '..'))

from plugin_constants import MAX_RECORDED_COUNT, SIMULATOR_FOLDERS
import iossim_util

LOGGER = logging.getLogger(__name__)


class BasePlugin(object):
  """ Base plugin class """

  def __init__(self, device_info_cache, out_dir):
    """ Initializes a new instance of this class.

    Args:
      device_info_cache: a dictionary where keys are device names and values are
      dictionaries of information about that testing device. A single
      device_info_cache can be shared between multiple plugins so that those
      plugins can share state.
      out_dir: output directory for saving any useful data

    """
    self.device_info_cache = device_info_cache
    self.out_dir = out_dir

  def test_case_will_start(self, request):
    """ Optional method to implement when a test case is about to start """
    pass

  def test_case_did_finish(self, request):
    """ Optional method to implement when a test case is finished executing

    Note that this method will always be called at the end of a test case
    execution, regardless whether a test case failed or not.
    """
    pass

  def test_case_did_fail(self, request):
    """ Optional method to implement when a test case failed unexpectedly

    Note that this method is being called right before test_case_did_finish,
    if the test case failed unexpectedly.
    """
    pass

  def test_bundle_will_finish(self, request):
    """ Optional method to implement when a test bundle will finish

    Note that this method will be called exactly once at the very end of the
    testing process.
    """
    pass

  def reset(self):
    """
    Optional method to implement to reset any running process/state
    in between each test attempt
    """
    pass

  def start_proc(self, cmd):
    """ Starts a non-block process

    Args:
      cmd: the shell command to be executed

    """
    LOGGER.info('Executing command: %s', cmd)
    return subprocess.Popen(cmd)

  def get_udid_and_path_for_device_name(self,
                                        device_name,
                                        paths=SIMULATOR_FOLDERS):
    """ Get the udid and path for a device name.

    Will first check self.devices if the device name exists, if not call
    simctl for well known paths to try to find it.

    Args:
      device_name: A device name as a string
    Returns:
      (UDID, path): A tuple of strings with representing the UDID of
      the device and path the location of the simulator. If the device is not
      able to be found (None, None) will be returned.
    """
    if self.device_info_cache.get(
        device_name) and self.device_info_cache[device_name].get(
            'UDID') and self.device_info_cache[device_name].get('path'):
      LOGGER.info('Found device named %s in cache. UDID: %s PATH: %s',
                  device_name, self.device_info_cache[device_name]['UDID'],
                  self.device_info_cache[device_name]['path'])
      return (self.device_info_cache[device_name]['UDID'],
              self.device_info_cache[device_name]['path'])

    # search over simulators to find device name
    # loop over paths
    for path in paths:
      for _runtime, simulators in iossim_util.get_simulator_list(
          path)['devices'].items():
        for simulator in simulators:
          if simulator['name'] == device_name:
            if not self.device_info_cache.get(device_name):
              self.device_info_cache[device_name] = {}
            self.device_info_cache[device_name]['UDID'] = simulator['udid']
            self.device_info_cache[device_name]['path'] = path
            return (simulator['udid'], path)
    # Return none if not found
    return (None, None)


class VideoRecorderPlugin(BasePlugin):
  """ Video plugin class for recording test execution """

  def __init__(self, device_info_cache, out_dir):
    """ Initializes a new instance of this class, which is a subclass
    of BasePlugin

    Args:
      device_info_cache: a dictionary where keys are device names and values are
      dictionaries of information about that testing device. A single
      device_info_cache can be shared between multiple plugins so that those
      plugins can share state.
      out_dir: output directory where the video plugin should be saved to

    """
    super(VideoRecorderPlugin, self).__init__(device_info_cache, out_dir)

    self.testcase_recorded_count = {}
    self.device_recording_process_map = {}

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
    udid, path = self.get_udid_and_path_for_device_name(
        request.device_info.name)
    recording_process = self.recording_process_for_device_name(
        request.device_info.name)
    if (recording_process.process != None):
      LOGGER.warning(
          'Previous recording for test case %s is still ongoing, '
          'terminating before starting new recording...',
          recording_process.test_case_name)
      self.stop_recording(False, recording_process)

    file_name = self.get_video_file_name(request.test_case_info.name,
                                         attempt_count)
    file_dir = os.path.join(self.out_dir, file_name)
    cmd = [
        'xcrun', 'simctl', '--set', path, 'io', udid, 'recordVideo',
        '--codec=h264', '-f', file_dir
    ]
    process = self.start_proc(cmd)
    recording_process.process = process
    recording_process.test_case_name = request.test_case_info.name

  def test_case_did_fail(self, request):
    """ Executes when a test class fails unexpectedly...

    This method will terminate the existing running video recording process
    iff the test name in the request matches the existing running process's
    test name.
    It will also save the video file to local disk (by default).
    Otherwise, it will do nothing.
    """
    recording_process = self.recording_process_for_device_name(
        request.device_info.name)
    if (request.test_case_info.name == recording_process.test_case_name):
      self.stop_recording(True, recording_process)
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
    recording_process = self.recording_process_for_device_name(
        request.device_info.name)
    if (request.test_case_info.name == recording_process.test_case_name):
      self.stop_recording(False, recording_process)
      recording_process.reset()
    else:
      LOGGER.warning('No video recording process is currently running for %s',
                     request.test_case_info.name)

  def stop_recording(self, should_save: bool,
                     recording_process: 'RecordingProcess'):
    """ Terminate existing running video recording process

    Args:
      shouldSave: required flag to decide whether the recorded vide should
      be saved to local disk.
      recording_process: the recording process that should be halted

    """
    LOGGER.info('Terminating video recording process for test case %s',
                recording_process.test_case_name)
    if not should_save:
      # SIGTERM will immediately terminate the process, and the video
      # file will be left corrupted. We will still need to delete the
      # corrupted video file.
      os.kill(recording_process.process.pid, signal.SIGTERM)
      attempt_count = self.testcase_recorded_count.get(
          recording_process.test_case_name, 0)
      file_name = self.get_video_file_name(recording_process.test_case_name,
                                           attempt_count)
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
      os.kill(recording_process.process.pid, signal.SIGINT)

    recording_process.reset()

  def recording_process_for_device_name(self,
                                        device_name: str) -> 'RecordingProcess':
    return self.device_recording_process_map.setdefault(device_name,
                                                        RecordingProcess())

  def reset(self):
    """ Executes in between each test attempet to reset any running
    process/state.

    This method will stop existing running video recording process,
    if there is any.
    """
    LOGGER.info('Clearing any running processes...')
    for recording_process in self.device_recording_process_map.values():
      if recording_process.process != None:
        self.stop_recording(False, recording_process)

  def get_video_file_name(self, test_case_name, attempt_count):
    # Remove all non-word characters (everything except numbers and letters)
    s = re.sub(r"[^\w\s]", '', test_case_name)
    # replace all whitespace with underscore
    s = re.sub(r"\s+", '_', s)
    # add attempt num at the beginning, Video at the end
    s = 'attempt_' + str(attempt_count) + '_' + s + '_Video.mov'
    return s


class FileCopyPlugin(BasePlugin):
  """ File Copy Plugin. Copies files from simulator at end of test execution """

  def __init__(self, glob_pattern, out_dir, device_info_cache):
    """ Initializes a file copy plugin which will copy all files matching
    the glob pattern to the dest_dir

    Args:
      glob_pattern: (str) globbing pattern to match files to pull from simulator
        The pattern is relative to the simulator's directory. So a globing
        for profraw files in the data directory of the simulator would be
        `data/*.profraw`
      out_dir: (str) Destination directory, it will be created if it doesn't
        exist
    """
    super(FileCopyPlugin, self).__init__(device_info_cache, out_dir)
    self.glob_pattern = glob_pattern

  def __str__(self):
    return "FileCopyPlugin. Glob: {}, Dest: {}".format(self.glob_pattern,
                                                       self.out_dir)

  def test_bundle_will_finish(self, request):
    """ Called just as a test bundle will finish.
    """
    UDID, path = self.get_udid_and_path_for_device_name(
        request.device_info.name)

    if not UDID or not path:
      LOGGER.warning("Can not find udid for device %s in paths %s",
                     request.device_info.name, SIMULATOR_FOLDERS)
      return
    if not os.path.exists(self.out_dir):
      os.mkdir(self.out_dir)
    for file in glob.glob(os.path.join(path, UDID, self.glob_pattern)):
      shutil.move(file, self.out_dir)


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

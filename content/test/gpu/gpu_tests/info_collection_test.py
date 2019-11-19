# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from gpu_tests import gpu_integration_test

import sys

class InfoCollectionTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    return 'info_collection'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    super(InfoCollectionTest, cls).AddCommandlineArgs(parser)
    parser.add_option('--expected-device-id',
        help='The expected device id')
    parser.add_option('--expected-vendor-id',
        help='The expected vendor id')

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('InfoCollection_basic', '_',
           ('_RunBasicTest',
            options.expected_vendor_id,
            options.expected_device_id))
    yield ('InfoCollection_direct_composition', '_',
           ('_RunDirectCompositionTest', '_', '_'))
    yield ('InfoCollection_dx12_vulkan', '_',
           ('_RunDX12VulkanTest', '_', '_'))

  @classmethod
  def SetUpProcess(cls):
    super(cls, InfoCollectionTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()

  def RunActualGpuTest(self, test_path, *args):
    # Make sure the GPU process is started
    self.tab.action_runner.Navigate('chrome:gpu')

    # Gather the IDs detected by the GPU process
    system_info = self.browser.GetSystemInfo()
    if not system_info:
      self.fail("Browser doesn't support GetSystemInfo")

    test_func = args[0]
    getattr(self, test_func)(system_info.gpu, args[1], args[2])

  ######################################
  # Helper functions for the tests below

  def _RunBasicTest(self, gpu, expected_vendor_id_str,
                    expected_device_id_str):
    device = gpu.devices[0]
    if not device:
      self.fail("System Info doesn't have a gpu")

    detected_vendor_id = device.vendor_id
    detected_device_id = device.device_id

    # Gather the expected IDs passed on the command line
    if expected_vendor_id_str is None or expected_device_id_str is None:
      self.fail("Missing --expected-[vendor|device]-id command line args")

    expected_vendor_id = int(expected_vendor_id_str, 16)
    expected_device_id = int(expected_device_id_str, 16)

    # Check expected and detected GPUs match
    if detected_vendor_id != expected_vendor_id:
      self.fail('Vendor ID mismatch, expected %s but got %s.' %
          (expected_vendor_id, detected_vendor_id))

    if detected_device_id != expected_device_id:
      self.fail('Device ID mismatch, expected %s but got %s.' %
          (expected_device_id, detected_device_id))

  def _RunDirectCompositionTest(self, gpu, unused_arg_0, unused_arg_1):
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      overlay_bot_config = self.GetOverlayBotConfig()
      aux_attributes = gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')
      for field, expected in overlay_bot_config.iteritems():
        detected = aux_attributes.get(field, 'NONE')
        if expected != detected:
          self.fail('%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  def _RunDX12VulkanTest(self, unused_arg_0, unused_arg_1, unused_arg_2):
    os_name = self.browser.platform.GetOSName()
    if os_name and os_name.lower() == 'win':
      self.RestartBrowserIfNecessaryWithArgs([
        '--no-delay-for-dx12-vulkan-info-collection'])
      # Need to re-request system info for DX12/Vulkan bits.
      system_info = self.browser.GetSystemInfo()
      if not system_info:
        self.fail("Browser doesn't support GetSystemInfo")
      gpu = system_info.gpu
      if gpu is None:
        raise Exception("System Info doesn't have a gpu")
      aux_attributes = gpu.aux_attributes
      if not aux_attributes:
        self.fail('GPU info does not have aux_attributes.')

      dx12_vulkan_bot_config = self.GetDx12VulkanBotConfig()
      for field, expected in dx12_vulkan_bot_config.iteritems():
        detected = aux_attributes.get(field)
        if expected != detected:
          self.fail('%s mismatch, expected %s but got %s.' %
              (field, self._ValueToStr(expected), self._ValueToStr(detected)))

  @staticmethod
  def _ValueToStr(value):
    if type(value) is str:
      return value
    if type(value) is unicode:
      return str(value)
    if type(value) is bool:
      return 'supported' if value else 'unsupported'
    assert False

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])

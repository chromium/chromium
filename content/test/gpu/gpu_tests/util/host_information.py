# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper functions for getting information about the host.

Most of the time, information should be pulled from the browser, e.g. via
GpuIntegrationTest.GetPlatformTags(), as this will be more accurate and supports
remote platforms. However, for cases where the browser isn't available such as
when determining serial tests, these functions can be used as stand-ins.
"""

import collections
import functools
import logging
import platform
import re
import shlex
import subprocess
import sys
from typing import Any, List

from gpu_tests import constants

if sys.platform == 'win32':
  # pylint: disable=import-error
  import win32com.client  # type: ignore
  # pylint: enable=import-error
elif sys.platform == 'darwin':
  import plistlib

_WMI_DEFAULT_NAMESPACE = 'root\\cimv2'

# The string looks like:
#  PCI\VEN_15AD&DEV_0405&SUBSYS_040515AD&REV_00\3&2B8E0B4B&0&78
# Qualcomm shows up as QCOM instead of a regular hex string, likely due to the
# hardware being an integrated SoC instead of using PCI-e.
_PNP_VENDOR_REGEX = re.compile(r'VEN_([0-9A-F]{4}|QCOM)')
_PNP_DEVICE_REGEX = re.compile(r'DEV_([0-9A-F]{4})')

# Looks for display class as noted at http://wiki.osdev.org/PCI
_LSPCI_PCI_ID_REGEX = re.compile(r'^(.+?) \[([0-9a-f]{4})\]$')

_MAC_PCI_ID_REGEX = re.compile(r'\(0x([0-9a-f]{4})\)')
_MAC_VENDOR_NAME_REGEX = re.compile(r'sppci_vendor_([a-z]+)$')

# The format of Qualcomm device IDs retrieved via WMI is different from what
# Chrome extracts. This table translates to what Chrome produces.
# 043a = older Adreno 680/685/690 GPUs (such as Surface Pro X, Dell trybots)
# 0636 = Adreno 690 GPU (such as Surface Pro 9 5G)
# 0c36 = Adreno 741 GPU (such as Surface Pro 11th Edition)
_QUALCOMM_DEVICE_MAP = {
    '043a': '41333430',
    '0636': '36333630',
    '0c36': '36334330',
}

_Gpu = collections.namedtuple('Gpu', ['vendor_id', 'device_id'])


@functools.lru_cache(maxsize=1)
def IsWindows() -> bool:
  return sys.platform == 'win32'


@functools.lru_cache(maxsize=1)
def IsLinux() -> bool:
  return sys.platform.startswith('linux')


@functools.lru_cache(maxsize=1)
def IsMac() -> bool:
  return sys.platform == 'darwin'


@functools.lru_cache(maxsize=1)
def IsArmCpu() -> bool:
  native_arm = platform.machine().lower() in ('arm', 'arm64')
  # This is necessary for the case of running x86 Python on arm devices via
  # an emulator. In that case, platform.machine() will show up as an x86
  # processor.
  emulated_x86 = 'armv8' in platform.processor().lower()
  return native_arm or emulated_x86


@functools.lru_cache(maxsize=1)
def Isx86Cpu() -> bool:
  # This will start failing if we ever support another arch like RISC-V, but
  # this should be adequate until that actually becomes an issue.
  return not IsArmCpu()


@functools.lru_cache(maxsize=1)
def IsIntelGpu() -> bool:
  return _IsGpuVendorPresent(constants.GpuVendor.INTEL)


@functools.lru_cache(maxsize=1)
def IsAmdGpu() -> bool:
  return _IsGpuVendorPresent(constants.GpuVendor.AMD)


@functools.lru_cache(maxsize=1)
def IsNvidiaGpu() -> bool:
  return _IsGpuVendorPresent(constants.GpuVendor.NVIDIA)


@functools.lru_cache(maxsize=1)
def IsQualcommGpu() -> bool:
  return _IsGpuVendorPresent(constants.GpuVendor.QUALCOMM)


@functools.lru_cache(maxsize=1)
def IsAppleGpu() -> bool:
  return _IsGpuVendorPresent(constants.GpuVendor.APPLE)


def _IsGpuVendorPresent(gpu_vendor: constants.GpuVendor) -> bool:
  return any(gpu.vendor_id == gpu_vendor for gpu in _GetAvailableGpus())


@functools.lru_cache(maxsize=1)
def _GetAvailableGpus() -> List[_Gpu]:
  if IsWindows():
    return _GetAvailableGpusWindows()
  if IsLinux():
    return _GetAvailableGpusLinux()
  if IsMac():
    return _GetAvailableGpusMac()
  raise RuntimeError('Attempted to get GPUs on unknown platform')


@functools.lru_cache(maxsize=1)
def _GetWmiWbem() -> Any:
  # pytype: disable=name-error
  wmi_service = win32com.client.Dispatch('WbemScripting.SWbemLocator')
  # pytype: enable=name-error
  return wmi_service.ConnectServer('.', _WMI_DEFAULT_NAMESPACE)


@functools.lru_cache(maxsize=1)
def _GetAvailableGpusWindows() -> List[_Gpu]:
  # Effectively copied from Swarming's get_gpu() in api/platforms/win.py.
  wbem = _GetWmiWbem()
  gpus = []
  for device in wbem.ExecQuery('SELECT * FROM Win32_VideoController'):
    pnp_string = device.PNPDeviceID
    vendor_id = None
    device_id = None

    match = _PNP_VENDOR_REGEX.search(pnp_string)
    if match:
      vendor_id = match.group(1).lower()
      if vendor_id == 'qcom':
        vendor_id = constants.GpuVendor.QUALCOMM
      else:
        vendor_id = int(vendor_id, 16)
    else:
      continue

    match = _PNP_DEVICE_REGEX.search(pnp_string)
    if match:
      device_id = match.group(1).lower()
      if vendor_id == constants.GpuVendor.QUALCOMM:
        device_id = _QUALCOMM_DEVICE_MAP[device_id]
      device_id = int(device_id, 16)
    else:
      continue

    gpus.append(_Gpu(vendor_id, device_id))

  return gpus


def _lspci() -> List[List[str]]:
  """Returns list of PCI devices found.

  list(Bus, Type, Vendor [ID], Device [ID], extra...)
  """
  try:
    process = subprocess.run(['lspci', '-mm', '-nn'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             text=True,
                             check=True)
  except FileNotFoundError:
    logging.warning(
        'Failed to find lspci to enumerate GPUs. This is expected to happen '
        'when running on a host for a remote platform such as Android and can '
        'be safely ignored in those cases.')
    return []
  except subprocess.CalledProcessError:
    logging.warning(
        'Running lspci failed, cannot enumerate GPUs. This is expected to '
        'happen when running on a host for a remote platform such as Fuchsia '
        'and can be safely ignored in those cases.')
    return []
  return [shlex.split(line) for line in process.stdout.splitlines()]


@functools.lru_cache(maxsize=1)
def _GetAvailableGpusLinux() -> List[_Gpu]:
  # Effectively copied from Swarming's get_gpu() in api/platforms/linux.py.
  pci_devices = _lspci()
  gpus = []
  for device_entry in pci_devices:
    match = _LSPCI_PCI_ID_REGEX.match(device_entry[1])
    if not match:
      continue

    device_type = match.group(2)
    if not device_type or not device_type.startswith('03'):
      continue

    vendor_match = _LSPCI_PCI_ID_REGEX.match(device_entry[2])
    device_match = _LSPCI_PCI_ID_REGEX.match(device_entry[3])
    if not vendor_match or not device_match:
      continue

    vendor_id = int(vendor_match.group(2), 16)
    device_id = int(device_match.group(2), 16)
    gpus.append(_Gpu(vendor_id, device_id))
  return gpus


def _get_system_profiler(data_type: str) -> dict:
  process = subprocess.run(['system_profiler', data_type, '-xml'],
                           stdout=subprocess.PIPE,
                           check=True)
  plist = plistlib.loads(process.stdout)  # pytype: disable=name-error
  return plist[0].get('_items', [])


@functools.lru_cache(maxsize=1)
def _GetAvailableGpusMac() -> List[_Gpu]:
  gpu_list = []
  # Effectively copied from Swarming's get_gpu() in api/platforms/osx.py.
  # This applies to all helper functions called from here as well.
  for gpu in _get_system_profiler('SPDisplaysDataType'):
    if not 'spdisplays_device-id' in gpu:
      # Apple Silicon GPUs don't show up as PCI-e devices, so they require
      # separate detection code.
      gpu_list.append(_HandleAppleGpu(gpu))
    else:
      gpu_list.append(_HandleNonAppleGpu(gpu))
  return gpu_list


def _HandleAppleGpu(gpu: dict) -> _Gpu:
  if ('spdisplays_vendor' not in gpu
      or gpu['spdisplays_vendor'] != 'sppci_vendor_Apple'):
    raise RuntimeError('_HandleAppleGpu() called with non-Apple GPU')
  if 'sppci_model' not in gpu:
    raise RuntimeError('No model found for Apple GPU')

  # Expected value is something like "Apple M2".
  model = gpu['sppci_model'].lower()
  vendor_name, device_name = model.split(maxsplit=1)
  if vendor_name != 'apple':
    raise RuntimeError(f'Got non-Apple vendor name {vendor_name} for Apple GPU')
  if not device_name:
    raise RuntimeError('Did not get a device name for Apple GPU')

  return _Gpu(constants.GpuVendor.APPLE, device_name)


def _HandleNonAppleGpu(gpu: dict) -> _Gpu:
  device_id = int(gpu['spdisplays_device-id'][2:], 16)

  vendor_id = None
  if 'spdisplays_vendor-id' in gpu:
    # Should be NVIDIA.
    vendor_id = gpu['spdisplays_vendor-id'][2:]
    vendor_id = int(vendor_id, 16)
    assert vendor_id == constants.GpuVendor.NVIDIA
  elif 'spdisplays_vendor' in gpu:
    # Either Intel or AMD.
    match = _MAC_PCI_ID_REGEX.search(gpu['spdisplays_vendor'])
    if match:
      vendor_id = match.group(1)
      vendor_id = int(vendor_id, 16)
      assert vendor_id in (constants.GpuVendor.INTEL, constants.GpuVendor.AMD)

  # MacOS 10.13 and above stopped including the Vendor ID in the
  # spdisplays_vendor string, so infer it from the vendor name instead.
  if vendor_id is None:
    model_name = gpu['sppci_model']
    vendor_name = model_name.split(' ', 1)[0].upper()
    if _IsKnownVendorName(vendor_name):
      vendor_id = constants.GpuVendor[vendor_name]

  if vendor_id is None and 'spdisplays_vendor' in gpu:
    match = _MAC_VENDOR_NAME_REGEX.search(gpu['spdisplays_vendor'])
    if match:
      vendor_name = match.group(1).upper()
      if _IsKnownVendorName(vendor_name):
        vendor_id = constants.GpuVendor[vendor_name]

  if vendor_id is None:
    raise RuntimeError('Unable to determine GPU vendor ID. Raw GPU info: %s' %
                       gpu)

  return _Gpu(vendor_id, device_id)


def _IsKnownVendorName(vendor_name: str) -> bool:
  try:
    _ = constants.GpuVendor[vendor_name]
    return True
  except KeyError:
    return False

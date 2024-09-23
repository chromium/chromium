# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper module for shared code related to video overlays."""

import collections
import enum
import functools
import json
import logging
from typing import Any, Dict, Iterable, List, Optional, Union

import dataclasses  # Built-in, but pylint gives an ordering false positive

from gpu_tests import common_typing as ct
from gpu_tests import constants
from gpu_tests import gpu_helper

from telemetry.internal.platform import gpu_device


# These can be changed to enum.StrEnum once Python 3.11+ is used.
class OverlaySupport:
  SOFTWARE = 'SOFTWARE'
  DIRECT = 'DIRECT'
  SCALING = 'SCALING'
  NONE = 'NONE'

  HARDWARE_OVERLAY_MODES = [DIRECT, SCALING]


class PixelFormat:
  NV12 = 'NV12'
  YUY2 = 'YUY2'
  BGRA8 = 'BGRA'

  ALL_PIXEL_FORMATS = [NV12, YUY2, BGRA8]


class VideoRotation(enum.IntEnum):
  UNROTATED = 0
  ROT90 = 90
  ROT180 = 180
  ROT270 = 270


class PresentationMode:
  COMPOSED = 'COMPOSED'
  OVERLAY = 'OVERLAY'
  NONE = 'NONE'
  COMPOSITION_FAILURE = 'COMPOSITION_FAILURE'
  GET_STATISTICS_FAILED = 'GET_STATISTICS_FAILED'


class ZeroCopyCodec(enum.Enum):
  UNSPECIFIED = 0
  H264 = 1
  VP9 = 2


# Trace events used by Chrome corresponding to PresentationMode values.
class PresentationModeEvent(enum.IntEnum):
  # Defined by Chromium for internal testing use.
  GET_STATISTICS_FAILED = -1
  # These match DXGI_FRAME_PRESENTATION_MODE
  COMPOSED = 0
  OVERLAY = 1
  NONE = 2
  COMPOSITION_FAILURE = 3


def PresentationModeEventToStr(
    presentation_mode_event: Union[int, PresentationModeEvent]) -> str:
  try:
    event = PresentationModeEvent(presentation_mode_event)
    return getattr(PresentationMode, event.name)
  except ValueError:
    # Hit when not a valid PresentationModeEvent value.
    return f'{presentation_mode_event} (unknown)'


DriverConditional = collections.namedtuple('DriverConditional',
                                           ['operation', 'version'])


@dataclasses.dataclass
class ZeroCopyConfig:
  supports_scaled_video: bool = False
  supported_codecs: List[ZeroCopyCodec] = ct.EmptyList()


class GpuOverlayConfig:
  """Contains all the video overlay information for a single GPU."""

  def __init__(self):
    self.direct_composition = False
    self.supports_overlays = False
    self._driver_version: Optional[str] = None
    self._zero_copy_config = ZeroCopyConfig()

    self._possible_overlay_support: Dict[str, str] = {}
    self._driver_conditionals: Dict[str, Iterable[DriverConditional]] = {}
    self._supported_rotations: Dict[str, List[VideoRotation]] = {}

    self._force_composed_bgra8_driver_conditionals: Iterable[
        DriverConditional] = []

    for pixel_format in PixelFormat.ALL_PIXEL_FORMATS:
      self._possible_overlay_support[pixel_format] = OverlaySupport.SOFTWARE
      self._driver_conditionals[pixel_format] = []
      self._supported_rotations[pixel_format] = [VideoRotation.UNROTATED]

  def __eq__(self, other: Any):
    if not isinstance(other, self.__class__):
      return False

    self_dict = self.__dict__
    other_dict = other.__dict__
    if set(self_dict.keys()) != set(other_dict.keys()):
      return False

    return all(self_dict[k] == other_dict[k] for k in self_dict)

  def WithDirectComposition(self) -> 'GpuOverlayConfig':
    """Enables direct composition support via software."""
    self.direct_composition = True
    self.supports_overlays = True
    for key in self._possible_overlay_support:
      self._possible_overlay_support[key] = OverlaySupport.SOFTWARE
    return self

  def WithHardwareNV12Support(
      self,
      driver_conditionals: Optional[Iterable[DriverConditional]] = None,
      supported_rotations: Optional[List[VideoRotation]] = None
  ) -> 'GpuOverlayConfig':
    """Enables NV12 hardware support.

    Args:
      driver_conditionals: An optional Iterable of DriverConditional instances
          specifying when NV12 support is available. All conditionals must
          evaluate to true for support to be available. If not set, all drivers
          are assumed to be valid.
      supported_rotations: An optional List of supported video rotations for
          NV12 overlays. If not set, no video rotation support is assumed.
    """
    self._WithHardwareSupport(PixelFormat.NV12, driver_conditionals,
                              supported_rotations)
    return self

  def WithHardwareYUY2Support(
      self,
      driver_conditionals: Optional[Iterable[DriverConditional]] = None,
      supported_rotations: Optional[List[VideoRotation]] = None
  ) -> 'GpuOverlayConfig':
    """Enables YUY2 hardware support.

    Args:
      driver_conditionals: An optional Iterable of DriverConditional instances
          specifying when YUY2 support is available. All conditionals must
          evaluate to true for support to be available. If not set, all drivers
          are assumed to be valid.
      supported_rotations: An optional List of supported video rotations for
          YUY2 overlays. If not set, no video rotation support is assumed.
    """
    self._WithHardwareSupport(PixelFormat.YUY2, driver_conditionals,
                              supported_rotations)
    return self

  def WithHardwareBGRA8Support(
      self,
      driver_conditionals: Optional[Iterable[DriverConditional]] = None,
      supported_rotations: Optional[List[VideoRotation]] = None
  ) -> 'GpuOverlayConfig':
    """Enables BGRA8 hardware support.

    Args:
      driver_conditionals: An optional Iterable of DriverConditional instances
          specifying when BGRA8 support is available. All conditionals must
          evaluate to true for support to be available. If not set, all drivers
          are assumed to be valid.
      supported_rotations: An optional List of supported video rotations for
          BGRA8 overlays. If not set, no video rotation support is assumed.
    """
    self._WithHardwareSupport(PixelFormat.BGRA8, driver_conditionals,
                              supported_rotations)
    return self

  def _WithHardwareSupport(
      self,
      pixel_format: str,
      driver_conditionals: Optional[Iterable[DriverConditional]] = None,
      supported_rotations: Optional[List[VideoRotation]] = None) -> None:
    assert self.supports_overlays
    assert (
        self._possible_overlay_support[pixel_format] == OverlaySupport.SOFTWARE)
    self._driver_conditionals[pixel_format] = driver_conditionals or []
    self._supported_rotations[pixel_format].extend(supported_rotations or [])
    self._possible_overlay_support[pixel_format] = OverlaySupport.SCALING

  def WithForceComposedBGRA8(
      self,
      driver_conditionals: Iterable[DriverConditional]) -> 'GpuOverlayConfig':
    """Forces BGRA8 to use the COMPOSED presentation mode on certain drivers.

    In some cases, Chrome can report that software BGRA8 support is available,
    but BGRA8 presentation mode will still be COMPOSED instead of OVERLAY.

    Args:
      driver_conditionals: An Iterable of DriverConditional instances
          specifying when BGRA8 should be forced to use the COMPOSED
          presentation mode. Use of COMPOSED will be forced if any conditionals
          evaluate to true.
    """
    self._force_composed_bgra8_driver_conditionals = driver_conditionals
    return self

  def OnDriverVersion(self, driver_version: str) -> 'GpuOverlayConfig':
    """Specifies the driver version being used.

    If called multiple times, the same |driver_version| must be used every time.

    Args:
      driver_version: A string containing the driver version in use.
    """
    assert (self._driver_version is None
            or self._driver_version == driver_version)
    self._driver_version = driver_version
    return self

  def WithZeroCopyConfig(
      self, zero_copy_config: ZeroCopyConfig) -> 'GpuOverlayConfig':
    """Sets the ZeroCopyConfig to use."""
    self._zero_copy_config = zero_copy_config
    return self

  @functools.cached_property
  def nv12_overlay_support(self) -> str:
    return self._GetOverlaySupport(PixelFormat.NV12)

  @functools.cached_property
  def yuy2_overlay_support(self) -> str:
    return self._GetOverlaySupport(PixelFormat.YUY2)

  @functools.cached_property
  def bgra8_overlay_support(self) -> str:
    return self._GetOverlaySupport(PixelFormat.BGRA8)

  def _GetOverlaySupport(self, pixel_format: str) -> str:
    assert self._driver_version
    if not self.supports_overlays:
      return OverlaySupport.NONE

    for dc in self._driver_conditionals[pixel_format]:
      on_valid_driver = gpu_helper.EvaluateVersionComparison(
          self._driver_version, dc.operation, dc.version)
      if not on_valid_driver:
        return OverlaySupport.SOFTWARE

    return self._possible_overlay_support[pixel_format]

  @functools.cached_property
  def supports_hw_nv12_overlays(self) -> bool:
    return self.nv12_overlay_support in OverlaySupport.HARDWARE_OVERLAY_MODES

  @functools.cached_property
  def supports_hw_yuy2_overlays(self) -> bool:
    return self.yuy2_overlay_support in OverlaySupport.HARDWARE_OVERLAY_MODES

  @functools.cached_property
  def supports_hw_bgra8_overlays(self) -> bool:
    return self.bgra8_overlay_support in OverlaySupport.HARDWARE_OVERLAY_MODES

  @functools.cached_property
  def bgra8_should_be_composed(self) -> bool:
    assert self._driver_version
    if self.bgra8_overlay_support == OverlaySupport.NONE:
      return True
    for dc in self._force_composed_bgra8_driver_conditionals:
      on_invalid_driver = gpu_helper.EvaluateVersionComparison(
          self._driver_version, dc.operation, dc.version)
      if on_invalid_driver:
        return True
    return False

  def GetExpectedPixelFormat(self, forced_pixel_format: Optional[str]) -> str:
    """Retrieves the pixel format that is expected to be used for swap chains.

    Args:
      forced_pixel_format: An optional string specifying the pixel format that
          was forced in Chrome via browser arguments. If specified, should be
          a PixelFormat value.

    Returns:
      A string containing the PixelFormat that is expected to be used.
    """
    # If no specific pixel format was requested via browser arguments, we
    # expect use of NV12 > YUY2 > BGRA8 based on available hardware support.
    if forced_pixel_format is None:
      if self.supports_hw_nv12_overlays:
        return PixelFormat.NV12
      if self.supports_hw_yuy2_overlays:
        return PixelFormat.YUY2
      return PixelFormat.BGRA8

    assert forced_pixel_format in PixelFormat.ALL_PIXEL_FORMATS

    # If a specific pixel format was requested via browser arguments that the
    # browser does not support, we expect a direct fallback to BGRA8.
    if (forced_pixel_format == PixelFormat.NV12
        and not self.supports_hw_nv12_overlays):
      return PixelFormat.BGRA8
    if (forced_pixel_format == PixelFormat.YUY2
        and not self.supports_hw_yuy2_overlays):
      return PixelFormat.BGRA8
    return forced_pixel_format

  # pylint: disable=too-many-return-statements
  def GetExpectedPresentationMode(self, expected_pixel_format: str,
                                  video_rotation: VideoRotation) -> str:
    """Retrieves the presentation mode expected to be used for swap chains.

    Args:
      expected_pixel_format: A string containing the PixelFormat that is
          expected to be used for swap chains. Should be the value returned by
          GetExpectedPixelFormat().
      video_rotation: The rotation of the video being played.

    Returns:
      A string containing the PresentationMode that is expected to be used.
    """
    supported_rotations = self._supported_rotations[expected_pixel_format]
    # We expect NV12/YUY2 to use overlays unless the given video rotation is
    # not supported.
    if expected_pixel_format == PixelFormat.NV12:
      if video_rotation in supported_rotations:
        return PresentationMode.OVERLAY
      return PresentationMode.COMPOSED

    if expected_pixel_format == PixelFormat.YUY2:
      if video_rotation in supported_rotations:
        return PresentationMode.OVERLAY
      return PresentationMode.COMPOSED

    # We expect BGRA8 to always use overlays, even without hardware support.
    if expected_pixel_format == PixelFormat.BGRA8:
      if self.bgra8_should_be_composed:
        return PresentationMode.COMPOSED
      return PresentationMode.OVERLAY

    raise RuntimeError(
        f'Pixel format {expected_pixel_format} is known, but does not have '
        f'presentation mode logic added')

  # pylint: enable=too-many-return-statements

  def GetExpectedZeroCopyUsage(self, expected_pixel_format: str,
                               video_rotation: VideoRotation, fullsize: bool,
                               codec: ZeroCopyCodec) -> bool:
    """Determines whether the zero copy path is expected to be used or not.

    Args:
      expected_pixel_format: A string containing the PixelFormat that is
          expected to be used for swap chains.
      video_rotation: The rotation of the video being played.
      fullsize: Whether the video being played is at full size/unscaled or not.
      codec: The ZeroCopyCodec of the video being played. Can be UNSPECIFIED,
          but will be treated as an error if the codec actually ends up being
          needed to determine zero copy usage.

    Returns:
      True if the zero copy path is expected to be used, otherwise False.
    """
    # Rotated videos necessitate a copy.
    if video_rotation != VideoRotation.UNROTATED:
      return False

    # Zero copy path only enabled for NV12.
    if expected_pixel_format != PixelFormat.NV12:
      return False

    # Certain GPUs (namely from Intel) do not support zero copy if the video
    # is scaled at all.
    if not fullsize and not self._zero_copy_config.supports_scaled_video:
      return False

    if codec == ZeroCopyCodec.UNSPECIFIED:
      raise RuntimeError(
          'Test did not specify the codec used when it is relevant to '
          'determining zero copy usage')

    return codec in self._zero_copy_config.supported_codecs


BasicDirectCompositionConfig = lambda: (GpuOverlayConfig().
                                        WithDirectComposition())
AllHardwareSupportDirectCompositionConfig = lambda: (
    BasicDirectCompositionConfig()\
    .WithHardwareNV12Support()\
    .WithHardwareYUY2Support()\
    .WithHardwareBGRA8Support())

OVERLAY_CONFIGS = {
    constants.GpuVendor.AMD: {
        0x7340: BasicDirectCompositionConfig()\
                .WithHardwareNV12Support(supported_rotations=[
                    VideoRotation.ROT90,
                    VideoRotation.ROT180,
                    VideoRotation.ROT270])\
                .WithZeroCopyConfig(ZeroCopyConfig(
                    supports_scaled_video=True,
                    supported_codecs=[
                        ZeroCopyCodec.H264,
                        ZeroCopyCodec.VP9])),
        0x6613:
        BasicDirectCompositionConfig(),
        0x699f:
        BasicDirectCompositionConfig(),
    },
    constants.GpuVendor.INTEL: {
        # Hardware overlays are disabled in 26.20.100.8141 per
        # crbug.com/1079393#c105
        0x5912: BasicDirectCompositionConfig()\
                .WithHardwareNV12Support(driver_conditionals=[
                    DriverConditional('ne', '26.20.100.8141')])\
                .WithHardwareYUY2Support()\
                .WithHardwareBGRA8Support()\
                .WithZeroCopyConfig(ZeroCopyConfig(
                    supports_scaled_video=False,
                    supported_codecs=[
                        ZeroCopyCodec.H264,
                        ZeroCopyCodec.VP9])),
        0x3e92:
        AllHardwareSupportDirectCompositionConfig(),
        0x9bc5:
        AllHardwareSupportDirectCompositionConfig(),
        0x4680: BasicDirectCompositionConfig()\
                .WithHardwareNV12Support(supported_rotations=[
                    VideoRotation.ROT180])\
                .WithHardwareYUY2Support()\
                .WithHardwareBGRA8Support()\
                .WithZeroCopyConfig(ZeroCopyConfig(
                    supports_scaled_video=False,
                    supported_codecs=[
                        ZeroCopyCodec.H264])),
    },
    constants.GpuVendor.NVIDIA: {
        # For some reason, software BGRA8 software overlay support changes
        # based on driver version.
        0x2184: BasicDirectCompositionConfig()\
                .WithHardwareNV12Support(driver_conditionals=[
                    DriverConditional('ge', '31.0.15.4601')])\
                .WithHardwareYUY2Support(driver_conditionals=[
                    DriverConditional('ge', '31.0.15.4601')])
                .WithForceComposedBGRA8(driver_conditionals=[
                    DriverConditional('lt', '31.0.15.4601')])\
                .WithZeroCopyConfig(ZeroCopyConfig(
                    supports_scaled_video=True,
                    supported_codecs=[
                        ZeroCopyCodec.H264])),
        0x2783: BasicDirectCompositionConfig()\
                .WithHardwareNV12Support()\
                .WithHardwareYUY2Support()\
                .WithZeroCopyConfig(ZeroCopyConfig(
                    supports_scaled_video=True,
                    supported_codecs=[
                        ZeroCopyCodec.H264])),
    },
    constants.GpuVendor.QUALCOMM: {
        0x41333430: BasicDirectCompositionConfig()\
                    .WithHardwareNV12Support(supported_rotations=[
                        VideoRotation.ROT180])\
                    .WithZeroCopyConfig(ZeroCopyConfig(
                        supports_scaled_video=True)),
        0x36333630: BasicDirectCompositionConfig()\
                    .WithHardwareNV12Support(supported_rotations=[
                        VideoRotation.ROT180])\
                    .WithZeroCopyConfig(ZeroCopyConfig(
                        supports_scaled_video=True)),
        0x36334330: BasicDirectCompositionConfig()\
                    .WithHardwareNV12Support(supported_rotations=[
                        VideoRotation.ROT180])\
                    .WithZeroCopyConfig(ZeroCopyConfig(
                        supports_scaled_video=True)),
    },
}


def GetOverlayConfigForGpu(gpu: gpu_device.GPUDevice) -> GpuOverlayConfig:
  """Retrieves the GpuOverlayConfig instance for a particular GPU."""
  overlay_config = OVERLAY_CONFIGS.get(gpu.vendor_id,
                                       {}).get(gpu.device_id, None)
  if not overlay_config:
    raise RuntimeError(
        f'GPU with vendor ID {gpu.vendor_id:#02x} and device ID '
        f'{gpu.device_id:#02x} does not have an overlay config specified')
  return overlay_config.OnDriverVersion(gpu.driver_version)


def ParseOverlayJsonFile(filepath: str) -> None:
  """Parses overlay configs from the specified JSON file and adds to the map.

  JSON data must be in the following format:

  {
    # A string representing the vendor ID in hexadecimal, in this case Intel.
    "0x8086": {
      # A string representing the device ID in hexadecimal.
      "0x1234": [
        # An ordered list of functions to call on the GpuOverlayConfig.
        {
          # Equivalent to .WithDirectComposition()
          "function": "WithDirectComposition"
        },
        {
          # Equivalent to:
          # .WithHardwareNV12Support(
          #     driver_conditionals=[DriverConditional('ge', '31.0.15.4601')])
          "function": "WithHardwareNV12Support",
          "args": {
            "driver_conditionals": [
              ["ge", "31.0.15.4601"]
            ]
          }
        },
        {
          # Equivalent to:
          # .WithHardwareYUY2Support(supported_rotations=[VideoRotation.ROT180])
          "function": "WithHardwareYUY2Support",
          "args": {
            "supported_rotations": [
              180
            ]
          }
        },
        {
          # Equivalent to:
          # .WithZeroCopyConfig(ZeroCopyConfig(
          #     supports_scaled_video=True,
          #     supported_codecs=[ZeroCopyCodec.H264]))
          "function": "WithZeroCopyConfig",
          "args": {
            "supports_scaled_video": true,
            "supported_codecs": [
              'H264'
            ]
          }
        },
        {
          # Equivalent to:
          # .WithForceComposedBGRA8(
          #     driver_conditionals=[DriverConditional('ge', '31.0.15.4601')])
          "function": "WithForceComposedBGRA8",
          "args": {
            "driver_conditionals": [
              ["ge", "31.0.15.4601"]
            ]
          }
        }
      ],
      "0x2345": [
        ...
      ]
    },
    "0x1002": {
      ...
    }
  }

  Args:
    filepath: A string containing a filepath pointing to the JSON file to parse.
  """
  with open(filepath, encoding='utf-8') as infile:
    json_content = json.load(infile)

  for vendor_str, device_map in json_content.items():
    assert vendor_str.lower().startswith('0x')
    vendor = int(vendor_str, 0)
    vendor = constants.GpuVendor(vendor)

    for device_str, function_list in device_map.items():
      assert device_str.lower().startswith('0x')
      device = int(device_str, 0)
      _ParseOverlayJsonForDevice(vendor, device, function_list)


def _ParseOverlayJsonForDevice(vendor: constants.GpuVendor, device: int,
                               function_list: List[dict]) -> None:
  """Helper to parse overlay config JSON for a single device.

  Args:
    vendor: The GpuVendor value for the device's vendor.
    device: An int representing the device's ID.
    function_list: A list of dicts, each dict representing a function to call.
  """
  if device in OVERLAY_CONFIGS.get(vendor, {}):
    logging.warning(
        'Config for vendor %#02x and device %#02x already exists, not applying '
        'config from JSON file.', vendor, device)
    return

  overlay_config = GpuOverlayConfig()
  for function_call in function_list:
    function = function_call['function']
    args = function_call.get('args', {})
    # Convert any arguments that aren't built-in types before calling the
    # provided function with the provided args.
    _ConvertDriverConditionals(args)
    _ConvertSupportedRotations(args)
    _ConvertSupportedCodecs(args)
    if function == 'WithZeroCopyConfig':
      args = {'zero_copy_config': ZeroCopyConfig(**args)}
    getattr(overlay_config, function)(**args)

  OVERLAY_CONFIGS.setdefault(vendor, {})[device] = overlay_config


def _ConvertDriverConditionals(args: Dict[str, Any]) -> None:
  """Converts any driver_conditionals arguments in place."""
  if 'driver_conditionals' not in args:
    return

  driver_conditionals = [
      DriverConditional(*dc) for dc in args['driver_conditionals']
  ]
  args['driver_conditionals'] = driver_conditionals


def _ConvertSupportedRotations(args: Dict[str, Any]) -> None:
  """Converts any supported_rotations arguments in place."""
  if 'supported_rotations' not in args:
    return

  supported_rotations = [
      VideoRotation(sr) for sr in args['supported_rotations']
  ]
  args['supported_rotations'] = supported_rotations


def _ConvertSupportedCodecs(args: Dict[str, Any]) -> None:
  """Converts any supported_codecs arguments in place."""
  if 'supported_codecs' not in args:
    return

  supported_codecs = [ZeroCopyCodec[sc] for sc in args['supported_codecs']]
  args['supported_codecs'] = supported_codecs

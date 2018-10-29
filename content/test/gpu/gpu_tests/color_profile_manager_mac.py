# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# The Foundation and Quartz modules are opaque and will trigger no-member
# warnings on Mac. They will not exist on other platforms and will trigger
# import-error warnings.
# pylint: disable=no-member
# pylint: disable=import-error
# Variables will be pulled into globals() from the ColorSync framework, and will
# trigger undefined-variables.
# pylint: disable=undefined-variable

import sys
if sys.platform.startswith('darwin'):
  import Foundation
  import Quartz
  import objc
  # There is no module for the ColorSync framework, so synthesize one using
  # bridge # support.
  color_sync_framework = '/System/Library/Frameworks/ApplicationServices.' \
                         'framework/Versions/A/Frameworks/ColorSync.framework'
  # This string is the output of running gen_bridge_metadata on the ColorSync
  # headers.
  color_sync_bridge_string = """<?xml version='1.0'?>
    <signatures version='1.0'>
      <constant name='kColorSyncDeviceDefaultProfileID' type='^{__CFString=}'/>
      <constant name='kColorSyncDisplayDeviceClass' type='^{__CFString=}'/>
      <constant name='kColorSyncProfileUserScope' type='^{__CFString=}'/>
      <function name='CGDisplayCreateUUIDFromDisplayID'>
        <arg type='I'/>
        <retval already_retained='true' type='^{__CFUUID=}'/>
      </function>
      <function name='ColorSyncDeviceCopyDeviceInfo'>
        <arg type='^{__CFString=}'/>
        <arg type='^{__CFUUID=}'/>
        <retval already_retained='true' type='^{__CFDictionary=}'/>
      </function>
      <function name='ColorSyncDeviceSetCustomProfiles'>
        <arg type='^{__CFString=}'/>
        <arg type='^{__CFUUID=}'/>
        <arg type='^{__CFDictionary=}'/>
        <retval type='B'/>
      </function>
    </signatures>"""
  objc.parseBridgeSupport(color_sync_bridge_string, globals(),
                          color_sync_framework)

# Set |display_id| to use the color profile specified in |profile_url|. If
# |profile_url| is None, then use the factor default.
def SetDisplayCustomProfile(device_id, profile_url):
  if profile_url == None:
    profile_url = Foundation.kCFNull
  profile_info = {
    kColorSyncDeviceDefaultProfileID : profile_url,
    kColorSyncProfileUserScope : Foundation.kCFPreferencesCurrentUser
  }
  result = ColorSyncDeviceSetCustomProfiles(
              kColorSyncDisplayDeviceClass, device_id, profile_info)
  if result != True:
    raise

# Returns the URL for the system's sRGB color profile.
def GetSRGBProfileURL():
  srgb_profile_path = '/System/Library/ColorSync/Profiles/sRGB Profile.icc'
  srgb_profile_url = Foundation.CFURLCreateFromFileSystemRepresentation(
      None, srgb_profile_path, len(srgb_profile_path), False)
  return srgb_profile_url

# Return a map from display ID to custom color profiles set on the display or
# None if no custom color profile is set.
def GetDisplaysToProfileURLMap():
  display_profile_url_map = {}
  online_display_list_result = Quartz.CGGetOnlineDisplayList(32, None, None)
  error = online_display_list_result[0]
  if error != Quartz.kCGErrorSuccess:
    raise
  online_displays = online_display_list_result[1]
  for display_id in online_displays:
    device_info = ColorSyncDeviceCopyDeviceInfo(
        kColorSyncDisplayDeviceClass,
        CGDisplayCreateUUIDFromDisplayID(display_id))
    if not device_info:
      raise Exception('KVM connection on bot is broken, please file a bug')
    device_id = device_info['DeviceID']
    custom_profile_url = None
    if 'CustomProfiles' in device_info and '1' in device_info['CustomProfiles']:
      custom_profile_url = device_info['CustomProfiles']['1']
    display_profile_url_map[device_id] = custom_profile_url
  return display_profile_url_map

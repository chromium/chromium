# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import atexit

from gpu_tests.util import host_information


# Force all displays to use an sRGB color profile. By default, restore
# them at exit.
def ForceUntilExitSRGB(skip_restoring_color_profile: bool = False) -> None:
  if not host_information.IsMac():
    return
  if ForceUntilExitSRGB.has_forced_srgb:
    return
  ForceUntilExitSRGB.has_forced_srgb = True

  # pylint: disable=import-outside-toplevel
  from gpu_tests import color_profile_manager_mac
  # pylint: enable=import-outside-toplevel
  # Record the current color profiles.
  display_profile_url_map = \
      color_profile_manager_mac.GetDisplaysToProfileURLMap()
  # Force to sRGB.
  for display_id in display_profile_url_map:
    color_profile_manager_mac.SetDisplayCustomProfile(
        display_id, color_profile_manager_mac.GetSRGBProfileURL())
  # Register an atexit handler to restore the previous color profiles.
  def Restore():
    if skip_restoring_color_profile:
      print('Skipping restoring the original color profile')
      return
    for display_id in display_profile_url_map:
      color_profile_manager_mac.SetDisplayCustomProfile(
          display_id, display_profile_url_map[display_id])

  atexit.register(Restore)


ForceUntilExitSRGB.has_forced_srgb = False

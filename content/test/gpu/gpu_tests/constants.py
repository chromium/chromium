# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants used in GPU testing not specific to a particular module.

This module should **not** have any dependencies on any other GPU code to avoid
circular dependencies.
"""

import enum


class GpuVendor(enum.IntEnum):
  AMD = 0x1002
  APPLE = 0x106b
  INTEL = 0x8086
  NVIDIA = 0x10DE
  # ACPI ID as opposed to a PCI-E ID like other vendors.
  QUALCOMM = 0x4D4F4351

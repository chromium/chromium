# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def GetChromiumSrcDir():
  return os.path.abspath(
      os.path.join(
          os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
          os.pardir))


def GetGpuTestDir():
  return os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def GetCatapultDir():
  return os.path.abspath(
      os.path.join(GetChromiumSrcDir(), 'third_party', 'catapult'))


def AddDirToPathIfNeeded(*path_parts):
  path = os.path.abspath(os.path.join(*path_parts))
  if os.path.isdir(path) and path not in sys.path:
    sys.path.append(path)


def SetupTelemetryPaths():
  SetupToolsPerfPath()
  from core import path_util

  AddDirToPathIfNeeded(path_util.GetTelemetryDir())

  py_utils_path = os.path.join(GetCatapultDir(), 'common', 'py_utils')
  AddDirToPathIfNeeded(py_utils_path)


def SetupToolsPerfPath():
  AddDirToPathIfNeeded(GetChromiumSrcDir(), 'tools', 'perf')


def SetupTypPath():
  AddDirToPathIfNeeded(GetCatapultDir(), 'third_party', 'typ')

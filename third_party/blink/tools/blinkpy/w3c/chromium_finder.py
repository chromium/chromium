# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common.memoized import memoized
from blinkpy.common.path_finder import PathFinder


@memoized
def absolute_chromium_wpt_dir(filesystem):
    finder = PathFinder(filesystem)
    return finder.path_from_web_tests('external', 'wpt')


@memoized
def absolute_chromium_dir(filesystem):
    finder = PathFinder(filesystem)
    return finder.chromium_base()

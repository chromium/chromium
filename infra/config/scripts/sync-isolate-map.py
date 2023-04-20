#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Update the copy of gn_isolate_map.pyl in //testing/buildbot.

Until pinpoint is only building revisions that contain
//infra/config/generated/testing/gn_isolate_map.pyl,
//testing/buildbot/gn_isolate_map.pyl must exist and should be the same
as //infra/config/generated/testing/gn_isolate_map.pyl. This script
copies //infra/config/generated/testing/gn_isolate_map.pyl to
//testing/buildbot/gn_isolate_map.pyl and should be run after running
the starlark if it updates //testing/buildbot/gn_isolate_map.pyl.
"""

import os.path
import shutil

INFRA_CONFIG_DIR = os.path.normpath(f'{__file__}/../..')
TESTING_BUILDBOT_DIR = os.path.normpath(
    f'{INFRA_CONFIG_DIR}/../../testing/buildbot')

shutil.copyfile(f'{INFRA_CONFIG_DIR}/generated/testing/gn_isolate_map.pyl',
                f'{TESTING_BUILDBOT_DIR}/gn_isolate_map.pyl')

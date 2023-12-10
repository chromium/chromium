#!/usr/bin/env vpython3

# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script lists the features in RuntimeEnabledFeatures that are
# status: stable, with the first Chrome milestone in which they were
# status: stable.  This is useful for finding feature flags that we
# might be able to remove.

import os.path
import re
import sys

from blinkpy.common import path_finder
from blinkpy.common.system.filesystem import FileSystem
from blinkpy.common.checkout.git import Git

finder = path_finder.PathFinder(FileSystem())
sys.path.append(finder.path_from_chromium_base('third_party', 'pyjson5',
                                               'src'))
import json5


def version_to_ints(version):
    return list(map(int, version.split(".")))


def list_milestone_tags(git):
    """Return the latest (by numeric sort) tag for each Chrome milestone."""
    version_re = re.compile(r'^\d+\.\d+\.\d+\.\d+$')
    milestones = dict()
    for line in git.run(["tag", "-l"]).splitlines():
        line = line.strip()
        if version_re.match(line):
            version = version_to_ints(line)
            major = version[0]
            if not major in milestones or version_to_ints(
                    milestones[major]) < version:
                milestones[major] = line
    result = []
    for major in sorted(milestones.keys()):
        result.append(milestones[major])
    return result


def features_file(milestone=None):
    # These paths use / rather than os.path.join() because we (mostly)
    # want to pass them to git show.
    if (milestone is None or milestone >= 67):
        return "third_party/blink/renderer/platform/runtime_enabled_features.json5"

    # Moved in 0aee4434a4dba42a42abaea9bfbc0cd196a63bc1 (M67)

    if (milestone >= 63):
        return "third_party/WebKit/Source/platform/runtime_enabled_features.json5"

    # Moved in a9b51b33da012d575b69c1d535c85dd68a76c8ad (M63)

    if (milestone >= 58):
        return "third_party/WebKit/Source/platform/RuntimeEnabledFeatures.json5"

    # Converted to json5 in 5821741047894731b6527b607edfb8ddef33fd1b (M58)
    # return "third_party/WebKit/Source/platform/RuntimeEnabledFeatures.in"
    return None  # We can't parse it


def main():
    git = Git()
    with open(
            os.path.join(os.path.normpath(git.checkout_root),
                         os.path.normpath(features_file()))) as features_io:
        current_features_data = json5.load(features_io)["data"]
    stable_features = dict()
    for feature in current_features_data:
        if feature.get("status", "") == "stable":
            stable_features[feature["name"]] = "current"
    for version in reversed(list_milestone_tags(git)):
        major = version_to_ints(version)[0]
        features_filepath = features_file(major)
        if features_filepath is not None:
            old_file = git.run(
                ["show", "{}:{}".format(version, features_filepath)])
            old_data = json5.loads(old_file)["data"]
            for feature in old_data:
                name = feature["name"]
                if feature.get("status",
                               "") == "stable" and name in stable_features:
                    stable_features[name] = version
    for (feature, version) in sorted(
            stable_features.items(),
            key=lambda t: [1000000, 0, 0, 0, t[0]]
            if t[1] == "current" else version_to_ints(t[1]) + [t[0]]):
        print(version, feature)


if __name__ == "__main__":
    sys.exit(main())

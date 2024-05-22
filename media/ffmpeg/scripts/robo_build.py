#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Various functions to help build / test ffmpeg.

import glob
import os
import shutil
from robo_lib import shell


def ObliterateOldBuildOutputIfNeeded(robo_configuration):
    """Erase any build.*.* directories from build_ffmpeg if we haven't started
     the merge yet.
  Args:
    robo_configuration: RoboConfiguration.
  """

    # We have a sushi branch name if and only if we're on a sushi branch.  If
    # we're not, then we haven't done the initial merge from upstream yet, and
    # should clear the build directories.  Really, the idea is that any time we
    # merge from upstream, we should erase all of this.
    if robo_configuration.sushi_branch_name():
        shell.log(f"Skipping erase of build output - Already on sushi branch")
        return

    shell.log("Removing all build output from build_ffmpeg.py")
    robo_configuration.chdir_to_ffmpeg_home()
    for file in glob.glob("build.*.*"):
        robo_configuration.Call(["rm", "-r", file])


def ConfigureAndBuildFFmpeg(robo_configuration, platform, architecture):
    """Run FFmpeg's configure script, and build ffmpeg.

  Args:
    robo_configuration: RoboConfiguration.
    platform: platform name (e.g., "linux")
    architecture: (optional) arch name (e.g., "ia32").  If omitted, then we
          build all of them for |platform|.
  """
    shell.log("Generating FFmpeg config and building for %s %s" %
              (platform, architecture))

    shell.log("Starting FFmpeg build for %s %s" % (platform, architecture))
    robo_configuration.chdir_to_ffmpeg_home()
    # Include --fast so that we don't rebuild the same directory once we get it
    # right.  This saves time when only one platform is failing.
    build_script = robo_configuration.get_script_path("build_ffmpeg.py")
    command = ["python3", build_script, "--fast", platform]
    if architecture:
        command.append(architecture)
    if err := robo_configuration.Call(command, stdout=None, stderr=None):
        print(err)
        raise Exception("FFmpeg build failed for %s %s" %
                        (platform, architecture))


def ImportFFmpegConfigsIntoChromium(robo_configuration, write_git_file=False):
    """Import all FFmpeg configs that have been built so far and build gn files.

  Args:
    robo_configuration: RoboConfiguration.
    write_git_file: if true, then we'll ask generate_gn.py to write a script
    with the appropriate git commands to add / rm autorenames.
  """
    robo_configuration.chdir_to_ffmpeg_home()
    shell.log("Copying FFmpeg configs")
    CopyConfigPythonTranslation(robo_configuration)

    # TODO... seems like there are some auto-generated files that generate_gn.py
    # throws a nasty license check on, incorrectly. maybe they should be deleted
    shell.log("Generating GN config for all ffmpeg versions")
    generate_cmd = [robo_configuration.get_script_path("generate_gn.py")]
    if write_git_file:
        generate_cmd += ["-i", robo_configuration.autorename_git_file()]
    shell.log(generate_cmd)
    if robo_configuration.Call(generate_cmd):
        raise Exception("FFmpeg generate_gn.sh failed")


def CopyConfigPythonTranslation(robo_configuration):
    for opsys in ("android", "linux", "linux-noasm", "mac", "win"):
        for target in ("Chromium", "Chrome"):
            for arch in ("arm", "arm-neon", "arm64", "ia32", "x64", "mipsel",
                         "mips64el"):
                gen_dir = robo_configuration.target_config_directory(
                    arch, opsys, target)
                export_dir = robo_configuration.exported_configs_directory(
                    arch, opsys, target)
                if not os.path.exists(os.path.join(gen_dir, "config.h")):
                    continue  # Don't waste time on non-existent configs.
                    # if there is no config.h, skip.
                for file in (("config.h", ), ("config_components.h", ),
                             ("config.asm", ), ("libavutil", "avconfig.h"),
                             ("libavutil", "ffversion.h"), ("libavcodec",
                                                            "bsf_list.c"),
                             ("libavcodec", "codec_list.c"), ("libavcodec",
                                                              "parser_list.c"),
                             ("libavformat", "demuxer_list.c"),
                             ("libavformat",
                              "muxer_list.c"), ("libavformat",
                                                "protocol_list.c")):
                    copy_from = os.path.join(gen_dir, *file)
                    copy_to = os.path.join(export_dir, *file)
                    if os.path.exists(copy_from):
                        if not os.path.exists(os.path.dirname(copy_to)):
                            os.makedirs(os.path.dirname(copy_to))
                        print(f'CP {copy_from} {copy_to}')
                        shutil.copy(copy_from, copy_to)
            # Since we cannot cross-compile for ios, we just duplicate the mac config
            # for this platform.
            if opsys == "mac":
                mac_dir = robo_configuration.target_config_directory(
                    arch, opsys, "noarch")
                ios_dir = robo_configuration.target_config_directory(
                    arch, 'ios', "noarch")
                copy_from = os.path.dirname(mac_dir)
                if os.path.exists(copy_from):
                    copy_to = os.path.dirname(ios_dir)
                    shutil.copy(copy_from, copy_to)


def BuildAndImportAllFFmpegConfigs(robo_configuration):
    """Build ffmpeg for all platforms that we can, and build the gn files.

  Args:
    robo_configuration: RoboConfiguration.
  """
    if robo_configuration.host_operating_system() == "linux":
        ConfigureAndBuildFFmpeg(robo_configuration, "all", None)
    else:
        raise Exception("I don't know how to build ffmpeg for host type %s" %
                        robo_configuration.host_operating_system())

    # Now that we've built everything, import them and build the gn config.
    ImportFFmpegConfigsIntoChromium(robo_configuration, True)


# Build and import just the single ffmpeg version our host uses for testing.
def BuildAndImportFFmpegConfigForHost(robo_configuration):
    """Build and import FFmpeg for our host only.

  Build FFmpeg for our host, and create gn files for it.  This will probably
  produce autorename warnings which don't matter.

  This is useful for building local tests for the new ffmpeg.

  Args:
    robo_configuration: RoboConfiguration.
  """

    ConfigureAndBuildFFmpeg(robo_configuration,
                            robo_configuration.host_operating_system(),
                            robo_configuration.host_architecture())

    # Note that this will import anything that you've built, but that's okay.
    # Also note that we don't write the command file, since it's going to be
    # wrong.  Since we've only built some platforms, some autorenames may appear
    # to be no longer conflicting if they're not built on all platforms.
    ImportFFmpegConfigsIntoChromium(robo_configuration)


def BuildChromeTargetASAN(robo_configuration, target, platform, architecture):
    """Build a Chromium asan target.

  Args:
    robo_configuration: RoboConfiguration.
    target: chrome target to build (e.g., "media_unittests")
    platform: platform to build it for, which should probably be the host's.
    architecture: arch to build it for (e.g., "x64").
  """
    robo_configuration.chdir_to_chrome_src()
    if robo_configuration.Call([
            "autoninja", "-C",
            robo_configuration.relative_asan_directory(), target
    ]):
        raise Exception("Failed to build %s" % target)


def BuildAndRunChromeTargetASAN(robo_configuration, target, platform,
                                architecture):
    """Build and run a Chromium asan target.

  Args:
    robo_configuration: RoboConfiguration.
    target: chrome target to build (e.g., "media_unittests")
    platform: platform to build it for, which should probably be the host's.
    architecture: arch to build it for (e.g., "x64").
  """
    shell.log("Building and running %s" % target)
    BuildChromeTargetASAN(robo_configuration, target, platform, architecture)
    # TODO: we should be smarter about running things on android, for example.
    shell.log("Running %s" % target)
    robo_configuration.chdir_to_chrome_src()
    if robo_configuration.Call(
        [os.path.join(robo_configuration.absolute_asan_directory(), target)]):
        raise Exception("%s didn't complete successfully" % target)
    shell.log("%s ran successfully" % target)


def BuildChromex86(robo_configuration):
    """Build a Chromium target for x86 to make sure ffmpeg builds there.

  Args:
    robo_configuration: RoboConfiguration.
  """
    target = "media_unittests"
    # We choose chromeos rather than linux because x86 linux isn't supported.
    shell.log(f"Building x86 {target}")
    robo_configuration.chdir_to_chrome_src()
    if robo_configuration.Call([
            "autoninja", "-C",
            robo_configuration.relative_x86_directory(), target
    ]):
        raise Exception(f"Failed to build {target}")
    shell.log(f"x86 {target} built successfully")


def RunTests(robo_configuration):
    """Build all tests and run them locally.

  This assumes that the FFmpeg config and gn files are up to date for the host.
  If not, then please run BuildAndImportFFmpegConfigForHost first.

  Args:
    robo_configuration: RoboConfiguration.
  """
    host_operating_system = robo_configuration.host_operating_system()
    host_architecture = robo_configuration.host_architecture()
    BuildAndRunChromeTargetASAN(robo_configuration, "media_unittests",
                                host_operating_system, host_architecture)
    BuildAndRunChromeTargetASAN(robo_configuration, "ffmpeg_regression_tests",
                                host_operating_system, host_architecture)
    # chrome works, if you want to do some manual testing.
    #  BuildAndRunChromeTargetASAN(robo_configuration, "chrome",
    #          host_operating_system, host_architecture)

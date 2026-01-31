#!/usr/bin/env python3
#
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A tool to automate the process of rolling FFmpeg into Chromium.

This script orchestrates the following steps:
- Setting up the host environment.
- Merging upstream FFmpeg changes.
- Building configurations for all supported architectures.
- Running regression and unit tests.
- Generating patches and README files.
- Managing the git branch and upload process.
"""

import argparse
import sys
import typing

import robo_branch
from robo_lib import shell
from robo_lib import errors
from robo_lib import config
from robo_lib import task
import robo_build
import robo_setup
import robo_process


def main(argv):
    """Parses arguments and runs the requested build steps."""
    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument('--branch',
                        action='store',
                        help='Manually set sushi branch name')
    parser.add_argument('--prompt',
                        action='store_true',
                        help='Prompt for each robosushi step')
    parser.add_argument('--verbose',
                        action='store_true',
                        help='Log all shell calls')
    parser.add_argument(
        '--setup',
        action='store_true',
        help='Run initial setup steps. Do this once before auto-merge.')
    parser.add_argument('--test',
                        action='store_true',
                        help='Build and run all tests')
    parser.add_argument(
        '--build-gn',
        action='store_true',
        help='Unconditionally build all the configs and import them.')
    parser.add_argument('--auto-merge',
                        action='store_true',
                        help='Run auto-merge. (Usually what you want)')
    parser.add_argument('--list', action='store_true', help='List steps')
    parser.add_argument('--no-skip',
                        action='store_true',
                        help='Don\'t allow any steps to be skipped')
    parser.add_argument('--force-gn-rebuild',
                        action='store_true',
                        help='Force rebuild of GN args.')
    parser.add_argument('--step', action='append', help='Step to run.')

    options = parser.parse_args(argv)

    if options.list:
        task.RenderTasks()
        return 0

    robo_configuration = config.RoboConfiguration()
    robo_configuration.chdir_to_ffmpeg_home()

    shell.enable_file_logging(
        robo_configuration.get_script_path("robosushi.log"))

    exec_steps = []

    if options.prompt:
        robo_configuration.set_prompt_on_call(True)
    if options.verbose:
        robo_configuration.set_log_shell_calls(True)
    if options.no_skip:
        robo_configuration.set_skip_allowed(False)
    if options.force_gn_rebuild:
        robo_configuration.set_force_gn_rebuild()
    if options.branch:
        robo_configuration.SetBranchName(options.branch)

    if options.setup:
        exec_steps += ["setup"]
    if options.test:
        robo_build.BuildAndImportFFmpegConfigForHost(robo_configuration)
        robo_build.RunTests(robo_configuration)
    if options.build_gn:
        # Unconditionally build all the configs and import them.
        robo_build.BuildAndImportAllFFmpegConfigs(robo_configuration)
    if options.auto_merge:
        exec_steps += ["auto-merge"]
    if options.step:
        exec_steps += options.step

    if len(exec_steps) == 0:
        parser.print_help()
        return 1

    # TODO(crbug.com/450394703): make sure that any untracked autorename files
    # are removed, or make sure that the autorename git script doesn't try to
    # 'git rm' untracked files, else the script fails.
    task.RunTasks(robo_configuration,
                  [task.Task.Lookup(step) for step in exec_steps])

    # TODO(crbug.com/450394703): Start a fake deps roll.  To do this, we would:
    # Create new remote branch from the current remote sushi branch.
    # Create and check out a new local branch at the current local branch.
    # Make the new local branch track the new remote branch.
    # Push to origin/new remote branch.
    # Start a fake deps roll CL that runs the *san bots.
    # Switch back to original local branch.
    # For extra points, include a pointer to the fake deps roll CL in the
    # local branch, so that when it's pushed for review, it'll point the
    # reviewer at it.
    # TODO(crbug.com/450394703): git cl upload for review.


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except errors.UserInstructions as e:
        shell.log(e, style=(shell.Style.RED + shell.Style.BOLD))

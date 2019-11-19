# Copyright (c) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys

from blinkpy.common.memoized import memoized


def add_typ_dir_to_sys_path():
    path_to_typ = get_typ_dir()
    if path_to_typ not in sys.path:
        sys.path.insert(0, path_to_typ)


def add_bindings_scripts_dir_to_sys_path():
    path_to_bindings_scripts = get_bindings_scripts_dir()
    if path_to_bindings_scripts not in sys.path:
        sys.path.insert(0, path_to_bindings_scripts)


def add_build_scripts_dir_to_sys_path():
    path_to_build_scripts = get_build_scripts_dir()
    if path_to_build_scripts not in sys.path:
        sys.path.insert(0, path_to_build_scripts)


def add_blinkpy_thirdparty_dir_to_sys_path():
    path = get_blinkpy_thirdparty_dir()
    if path not in sys.path:
        sys.path.insert(0, path)


def add_depot_tools_dir_to_os_path():
    path = get_depot_tools_dir()
    if path not in os.environ['PATH']:
        os.environ['PATH'] += os.pathsep + path


def get_bindings_scripts_dir():
    return os.path.join(get_source_dir(), 'bindings', 'scripts')


def get_blink_dir():
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__)))))


def get_chromium_src_dir():
    return os.path.dirname(os.path.dirname(get_blink_dir()))


def get_depot_tools_dir():
    return os.path.join(get_chromium_src_dir(), 'third_party', 'depot_tools')


def get_source_dir():
    return os.path.join(get_chromium_src_dir(), 'third_party', 'blink', 'renderer')


def get_typ_dir():
    return os.path.join(get_chromium_src_dir(), 'third_party', 'catapult',
                        'third_party', 'typ')


def get_blinkpy_thirdparty_dir():
    return os.path.join(get_blink_tools_dir(), 'blinkpy', 'third_party')


def get_blink_tools_dir():
    return os.path.join(get_chromium_src_dir(), 'third_party', 'blink', 'tools')


def get_build_scripts_dir():
    return os.path.join(get_source_dir(), 'build', 'scripts')


def add_blink_tools_dir_to_sys_path():
    path = get_blink_tools_dir()
    if path not in sys.path:
        sys.path.insert(0, path)


def _does_blink_web_tests_exist():
    return os.path.exists(os.path.join(get_chromium_src_dir(), 'third_party',
                                       'blink', 'web_tests'))


TESTS_IN_BLINK = _does_blink_web_tests_exist()
# web_tests path relative to the repository root.
# Path separators are always '/', and this contains the trailing '/'.
RELATIVE_WEB_TESTS = 'third_party/blink/web_tests/'
WEB_TESTS_LAST_COMPONENT = 'web_tests'

class PathFinder(object):

    def __init__(self, filesystem, sys_path=None, env_path=None):
        self._filesystem = filesystem
        self._dirsep = filesystem.sep
        self._sys_path = sys_path or sys.path
        self._env_path = env_path or os.environ['PATH'].split(os.pathsep)

    @memoized
    def chromium_base(self):
        return self._filesystem.dirname(self._filesystem.dirname(self._blink_base()))

    def web_tests_dir(self):
        return self.path_from_chromium_base('third_party', 'blink', 'web_tests')

    def perf_tests_dir(self):
        return self.path_from_chromium_base('third_party', 'blink', 'perf_tests')

    def webdriver_prefix(self):
        return self._filesystem.join('external', 'wpt', 'webdriver', '')

    @memoized
    def _blink_base(self):
        """Returns the absolute path to the top of the Blink directory."""
        module_path = self._filesystem.path_to_module(self.__module__)
        tools_index = module_path.rfind('tools')
        assert tools_index != -1, 'could not find location of this checkout from %s' % module_path
        return self._filesystem.normpath(module_path[0:tools_index - 1])

    def path_from_chromium_base(self, *comps):
        return self._filesystem.join(self.chromium_base(), *comps)

    def _blink_source_dir(self):
        return self._filesystem.join(
            self.chromium_base(), 'third_party', 'blink', 'renderer')

    def path_from_blink_source(self, *comps):
        return self._filesystem.join(self._blink_source_dir(), *comps)

    def path_from_blink_tools(self, *comps):
        return self._filesystem.join(self._filesystem.join(self.chromium_base(), 'third_party', 'blink', 'tools'), *comps)

    def path_from_web_tests(self, *comps):
        return self._filesystem.join(self.web_tests_dir(), *comps)

    def strip_web_tests_path(self, wpt_test_abs_path):
        web_tests_path = self.path_from_web_tests('')
        if wpt_test_abs_path.startswith(web_tests_path):
            return wpt_test_abs_path[len(web_tests_path):]
        return wpt_test_abs_path

    def strip_webdriver_tests_path(self, wpt_webdriver_test_path):
        if self.is_webdriver_test_path(wpt_webdriver_test_path):
            return wpt_webdriver_test_path[len(self.webdriver_prefix()):]
        return wpt_webdriver_test_path

    def is_webdriver_test_path(self, test_path):
        return test_path.startswith(self.webdriver_prefix())

    @memoized
    def depot_tools_base(self):
        """Returns the path to depot_tools, or None if not found.

        Expects depot_tools to be //third_party/depot_tools.
        src.git's DEPS defines depot_tools to be there.
        """
        depot_tools = self.path_from_chromium_base('third_party', 'depot_tools')
        return depot_tools if self._filesystem.isdir(depot_tools) else None

    def path_from_depot_tools_base(self, *comps):
        return self._filesystem.join(self.depot_tools_base(), *comps)

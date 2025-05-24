#!/usr/bin/env python3
# Copyright (c) 2013 Google Inc. All rights reserved.
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
#
# Used for generating LayoutTest-compatible html files to run html5lib *.dat files.

import os
import glob


class WrapperGenerator(object):
    INPUT_DIRECTORY = "resources"
    INPUT_SUFFIX = ".dat"
    OUTPUT_DIRECTORY = "generated"
    HARNESS_PREFIX = "run-"
    HARNESS_SUFFIX = ".html"
    EXPECTAION_SUFFIX = "-expected.txt"
    HARNESS_TYPES = ("write", "data")

    HARNESS_TEMPLATE = """<!DOCTYPE html>
<script>
var test_files = [ '%(test_path)s' ]
</script>
<script src="../../resources/dump-as-markup.js"></script>
%(extra_content)s
<script src="../resources/runner.js"></script>
"""

    def _files_in_directory_with_suffix(self, directory, suffix):
        return glob.glob(os.path.join(directory, '*' + suffix))

    def _last_path_component_removing_suffix(self, path, suffix):
        return os.path.split(path)[-1][:-len(suffix)]

    def _remove_harness_prefix(self, name):
        assert(name.startswith(self.HARNESS_PREFIX))
        return name[len(self.HARNESS_PREFIX):]

    def _remove_harness_type(self, name):
        parts = name.split('-')
        assert(parts[-1] in self.HARNESS_TYPES)
        return "-".join(parts[:-1])

    def _test_name_from_harness_name(self, name):
        name = self._remove_harness_prefix(name)
        return self._remove_harness_type(name)

    def _remove_stale_tests(self, test_names):
        for path in self._files_in_directory_with_suffix(self.OUTPUT_DIRECTORY, self.HARNESS_SUFFIX):
            name = self._last_path_component_removing_suffix(path, self.HARNESS_SUFFIX)
            name = self._test_name_from_harness_name(name)
            if name not in test_names:
                print("Removing %s, %s no longer exists." % (path, self._input_path(name)))
                os.remove(path)

        for path in self._files_in_directory_with_suffix(self.OUTPUT_DIRECTORY, self.EXPECTAION_SUFFIX):
            name = self._last_path_component_removing_suffix(path, self.EXPECTAION_SUFFIX)
            name = self._test_name_from_harness_name(name)
            if name not in test_names:
                print("Removing %s, %s no longer exists." % (path, self._input_path(name)))
                os.remove(path)

    def _input_path(self, test_name):
        return os.path.join(self.INPUT_DIRECTORY, test_name + self.INPUT_SUFFIX)

    def _harness_path(self, test_name, use_write):
        harness_path = os.path.join(self.OUTPUT_DIRECTORY, self.HARNESS_PREFIX + test_name)
        if use_write:
            harness_path += "-write"
        else:
            harness_path += "-data"
        return harness_path + self.HARNESS_SUFFIX

    def _harness_content(self, test_name, use_write):
        extra_content = ""
        if not use_write:
            extra_content = "<script>window.forceDataURLs = true;</script>";
        return self.HARNESS_TEMPLATE % {
            # FIXME: .. should be relative to the number of components in OUTPUT_DIRECTORY
            'test_path': os.path.join('..', self._input_path(test_name)),
            'extra_content': extra_content,
        }

    def _write_harness(self, test_name, use_write):
        harness_file = open(self._harness_path(test_name, use_write), "w")
        harness_file.write(self._harness_content(test_name, use_write))

    def main(self):
        test_names = [self._last_path_component_removing_suffix(path, self.INPUT_SUFFIX) for path in self._files_in_directory_with_suffix(self.INPUT_DIRECTORY, self.INPUT_SUFFIX)]

        self._remove_stale_tests(test_names)

        for name in test_names:
            self._write_harness(name, True)
            self._write_harness(name, False)


if __name__ == "__main__":
    WrapperGenerator().main()

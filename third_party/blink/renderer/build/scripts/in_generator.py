# Copyright (C) 2013 Google Inc. All rights reserved.
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

from __future__ import print_function

import os
import os.path
import sys
import optparse

from in_file import InFile

# TODO(crbug.com/1174969): Remove this once Python2 is obsoleted.
if sys.version_info.major != 2:
    basestring = str


#########################################################
# This is now deprecated - use json5_generator.py instead
#########################################################
class GenericWriter(object):
    def __init__(self, in_files):
        self._outputs = {}  # file_name -> generator

    def _write_file_if_changed(self, output_dir, contents, file_name):
        path = os.path.join(output_dir, file_name)

        # The build system should ensure our output directory exists, but just in case.
        directory = os.path.dirname(path)
        if not os.path.exists(directory):
            os.makedirs(directory)

        # Only write the file if the contents have changed. This allows ninja to
        # skip rebuilding targets which depend on the output.
        with open(path, "a+") as output_file:
            output_file.seek(0)
            if output_file.read() != contents:
                output_file.truncate(0)
                output_file.write(contents)

    def write_files(self, output_dir):
        for file_name, generator in self._outputs.items():
            self._write_file_if_changed(output_dir, generator(), file_name)

    def set_gperf_path(self, gperf_path):
        self.gperf_path = gperf_path


class Writer(GenericWriter):
    # Subclasses should override.
    class_name = None
    defaults = None
    valid_values = None
    default_parameters = None

    def __init__(self, in_files):
        super(Writer, self).__init__(in_files)

        if isinstance(in_files, basestring):
            in_files = [in_files]
        if in_files:
            self.in_file = InFile.load_from_files(in_files, self.defaults,
                                                  self.valid_values,
                                                  self.default_parameters)
        else:
            self.in_file = None


class Maker(object):
    def __init__(self, writer_class):
        self._writer_class = writer_class

    def main(self, argv):
        script_name = os.path.basename(argv[0])
        args = argv[1:]
        if len(args) < 1:
            print("USAGE: %s INPUT_FILES" % script_name)
            exit(1)

        parser = optparse.OptionParser()

        parser.add_option("--gperf", default="gperf")
        parser.add_option("--output_dir", default=os.getcwd())
        options, args = parser.parse_args()

        writer = self._writer_class(args)
        writer.set_gperf_path(options.gperf)
        writer.write_files(options.output_dir)

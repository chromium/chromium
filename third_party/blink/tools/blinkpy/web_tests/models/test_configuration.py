# Copyright (C) 2011 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

import copy
import itertools
from functools import reduce


class TestConfiguration(object):
    def __init__(self, version, architecture, build_type):
        self.version = version
        self.architecture = architecture
        self.build_type = build_type

    @classmethod
    def category_order(cls):
        """The most common human-readable order in which the configuration properties are listed."""
        return ['version', 'architecture', 'build_type']

    def items(self):
        return list(self.__dict__.items())

    def keys(self):
        return list(self.__dict__.keys())

    def __str__(self):
        return (
            '<%(version)s, %(architecture)s, %(build_type)s>' % self.__dict__)

    def __repr__(self):
        return "TestConfig(version='%(version)s', architecture='%(architecture)s', build_type='%(build_type)s')" % self.__dict__

    def __hash__(self):
        return hash(self.version + self.architecture + self.build_type)

    def __eq__(self, other):
        return self.__hash__() == other.__hash__()

    def values(self):
        """Returns the configuration values of this instance as a tuple."""
        return list(self.__dict__.values())

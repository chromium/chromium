# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools

from blinkpy.common import path_finder

path_finder.add_typ_dir_to_sys_path()

from typ import host, json_results, expectations_parser, artifacts, result_sink
from typ.fakes import host_fake


# Some test names include spaces, which aren't allowed in typ's expectation
# test names. So, provide a way to encode/decode URI-encoded spaces.
def _uri_encode_spaces(s):
    s = s.replace('%', '%25')
    s = s.replace(' ', '%20')
    return s


def _uri_decode_spaces(s):
    s = s.replace('%20', ' ')
    s = s.replace('%25', '%')
    return s


# Adds symbols from typ that are used in blinkpy
Result = json_results.Result
ResultType = json_results.ResultType
# Automatically apply Blink's encoding/decoding to test names.
Expectation = functools.partial(expectations_parser.Expectation,
                                encode_func=_uri_encode_spaces)
TestExpectations = functools.partial(expectations_parser.TestExpectations,
                                     encode_func=_uri_encode_spaces,
                                     decode_func=_uri_decode_spaces)
# Type aliases for use with type hinting since the class references are
# overridden with partials.
ExpectationType = expectations_parser.Expectation
TestExpectationsType = expectations_parser.TestExpectations
Artifacts = artifacts.Artifacts
ResultSinkReporter = result_sink.ResultSinkReporter
RESULT_TAGS = expectations_parser.RESULT_TAGS
# This type has a different name to avoid naming clashes with
# `blinkpy.common.host.Host` when imported.


class SerializableTypHost(host.Host):
    """Production typ host that `pickle` can serialize.

    When serialized, this class drops references to file handles that cannot be
    transported between processes. The unpickled host will use the standard
    streams and logger of its current process.
    """

    def __reduce__(self):
        attrs = {'env': dict(self.env), 'platform': self.platform}
        return (self.__class__, (), attrs)


class FileSystemForwardingTypHost(host_fake.FakeHost):
    """Typ fake host that forwards calls that modify the filesystem.

    This class forwards calls to their equivalents on an internal Blink
    filesystem. This exposes filesystem changes by typ to Blink unit tests.

    Attributes:
        fs (FileSystem | MockFileSystem): Internal filesystem.
    """

    def __init__(self, filesystem, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.fs = filesystem

    def chdir(self, *comps):
        self.fs.chdir(self.fs.join(*comps))
        return super().chdir(*comps)

    def maybe_make_directory(self, *comps):
        self.fs.maybe_make_directory(*comps)
        return super().maybe_make_directory(*comps)

    def mktempfile(self, delete=True):
        self.mktemp()
        return super().mktempfile(delete=delete)

    def mkdtemp(self, suffix='', prefix='tmp', dir=None, **kwargs):
        self.fs.mkdtemp(suffix=suffix, prefix=prefix, dir=dir, **kwargs)
        return super().mkdtemp(suffix, prefix, dir, **kwargs)

    def remove(self, *comps):
        self.fs.remove(self.fs.join(*comps))
        return super().remove(*comps)

    def rmtree(self, *comps):
        self.fs.rmtree(self.fs.join(*comps))
        return super().rmtree(*comps)

    def write_binary_file(self, path, contents):
        self.fs.write_binary_file(path, contents)
        return super().write_binary_file(path, contents)

    def write_text_file(self, path, contents):
        self.fs.write_text_file(path, contents)
        return super().write_text_file(path, contents)

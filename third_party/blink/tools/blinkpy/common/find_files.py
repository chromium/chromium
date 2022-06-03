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
"""A utility module used to find files. It exposes one public function: find().

If a list is passed in, the returned list of files is constrained to those
found under the paths passed in. i.e. calling find(["web_tests/fast"])
will only return files under that directory.

If a set of skipped directories is passed in, the function will filter out
the files lying in these directories i.e. find(["web_tests"], set(["fast"]))
will return everything except files in the "fast" subdirectory.

If a callback is passed in, it will be called for the each file and the file
will be included into the result if the callback returns True.

The callback has to take three arguments: filesystem, dirname and filename.
"""

import itertools


def find(filesystem,
         base_dir,
         paths=None,
         skipped_directories=None,
         file_filter=None,
         directory_sort_key=None):
    """Finds the set of tests under a given list of sub-paths.

    Args:
        filesystem: A FileSystem instance.
        base_dir: A base directory to search under.
        paths: A list of path expressions relative to |base_dir|. Glob patterns
            are OK, as are path expressions with forward slashes on Windows.
            If paths is not given, we look at everything under |base_dir|.
        file_filter: A predicate function which takes three arguments:
            filesystem, dirname and filename.
        directory_sort_key: A sort key function.

    Returns:
        An iterable of absolute paths that were found.
    """
    paths = paths or ['*']
    skipped_directories = skipped_directories or set()
    absolute_paths = _normalize(filesystem, base_dir, paths)
    return _normalized_find(filesystem, absolute_paths, skipped_directories,
                            file_filter, directory_sort_key)


def _normalize(filesystem, base_dir, paths):
    return [
        filesystem.normpath(filesystem.join(base_dir, path)) for path in paths
    ]


def _normalized_find(filesystem, paths, skipped_directories, file_filter,
                     directory_sort_key):
    """Finds the set of tests under the given list of paths."""
    paths_to_walk = itertools.chain(*(filesystem.glob(path) for path in paths))

    def sort_by_directory_key(files_list):
        if directory_sort_key:
            files_list.sort(key=directory_sort_key)
        return files_list

    return itertools.chain(*(sort_by_directory_key(
        filesystem.files_under(path, skipped_directories, file_filter))
                             for path in paths_to_walk))

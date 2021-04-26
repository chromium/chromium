# Copyright (C) 2010 Google Inc. All rights reserved.
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
"""Generic routines to convert platform-specific paths to URIs."""

from six.moves import urllib


def abspath_to_uri(platform, path):
    """Converts a platform-specific absolute path to a file: URL."""
    return 'file:' + _escape(_convert_path(platform, path))


def _escape(path):
    """Handle any characters in the path that should be escaped."""
    # FIXME: web browsers don't appear to blindly quote every character
    # when converting filenames to files. Instead of using urllib's default
    # rules, we allow a small list of other characters through un-escaped.
    # It's unclear if this is the best possible solution.
    return urllib.parse.quote(path, safe='/+:')


def _convert_path(platform, path):
    """Handles any os-specific path separators, mappings, etc."""
    if platform.is_win():
        return _winpath_to_uri(path)
    return _unixypath_to_uri(path)


def _winpath_to_uri(path):
    """Converts a window absolute path to a file: URL."""
    return '///' + path.replace('\\', '/')


def _unixypath_to_uri(path):
    """Converts a unix-style path to a file: URL."""
    return '//' + path

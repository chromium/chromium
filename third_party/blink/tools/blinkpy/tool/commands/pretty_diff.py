# Copyright (c) 2010 Google Inc. All rights reserved.
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

import logging
import optparse
import sys
import tempfile
import urllib

from blinkpy.common.pretty_diff import prettify_diff
from blinkpy.common.system.executive import ScriptError
from blinkpy.tool.commands.command import Command

_log = logging.getLogger(__name__)


class PrettyDiff(Command):
    name = 'pretty-diff'
    help_text = 'Shows the pretty diff in the default browser'
    show_in_main_help = True

    def __init__(self):
        options = [
            optparse.make_option(
                '-g',
                '--git-commit',
                action='store',
                dest='git_commit',
                help=
                ('Operate on a local commit. If a range, the commits are squashed into one. <ref>.... '
                 'includes the working copy changes. UPSTREAM can be used for the upstream/tracking branch.'
                 ))
        ]
        super(PrettyDiff, self).__init__(options)
        self._tool = None

    def execute(self, options, args, tool):
        self._tool = tool
        pretty_diff_file = self._show_pretty_diff(options)
        if pretty_diff_file:
            diff_correct = tool.user.confirm('Was that diff correct?')
            pretty_diff_file.close()
            if not diff_correct:
                sys.exit(1)

    def _show_pretty_diff(self, options):
        if not self._tool.user.can_open_url():
            return None
        try:
            patch = self._diff(options)
            pretty_diff_file = PrettyDiff._pretty_diff_file(patch)
            self._open_pretty_diff(pretty_diff_file.name)
            # We return the pretty_diff_file here because we need to keep the
            # file alive until the user has had a chance to confirm the diff.
            return pretty_diff_file
        except ScriptError as error:
            _log.warning('PrettyPatch failed.  :(')
            _log.error(error.message_with_output())
            self._exit(error.exit_code or 2)
        except OSError:
            _log.warning('PrettyPatch unavailable.')

    def _diff(self, options):
        changed_files = self._tool.git().changed_files(options.git_commit)
        return self._tool.git().create_patch(
            options.git_commit, changed_files=changed_files)

    @staticmethod
    def _pretty_diff_file(diff):
        # |diff|'s type is |bytes| because it can contain multiple text files
        # of different encodings.
        assert isinstance(diff, bytes)
        diff_file = tempfile.NamedTemporaryFile(suffix='.html')
        # We deocode |diff| as UTF-8 anyway because we generate a UTF-8 HTML.
        diff_file.write(prettify_diff(diff.decode(errors='replace')).encode())
        diff_file.flush()
        return diff_file

    def _open_pretty_diff(self, file_path):
        url = 'file://%s' % urllib.parse.quote(file_path)
        self._tool.user.open_url(url)

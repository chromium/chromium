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
import re
import six
from six.moves import cPickle
from typing import ClassVar

from blinkpy.web_tests.controllers import repaint_overlay
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.models.failure_reason import FailureReason
from blinkpy.common.html_diff import html_diff
from blinkpy.common.unified_diff import unified_diff

# TODO(weizhong) Create a unit test for each Failure type and make
# sure each artifact is written to the correct path

# Filename pieces when writing failures to the test results directory.
FILENAME_SUFFIX_ACTUAL = "-actual"
FILENAME_SUFFIX_EXPECTED = "-expected"
FILENAME_SUFFIX_CMD = "-command"
FILENAME_SUFFIX_DIFF = "-diff"
FILENAME_SUFFIX_STDERR = "-stderr"
FILENAME_SUFFIX_CRASH_LOG = "-crash-log"
FILENAME_SUFFIX_SAMPLE = "-sample"
FILENAME_SUFFIX_LEAK_LOG = "-leak-log"
FILENAME_SUFFIX_HTML_DIFF = "-pretty-diff"
FILENAME_SUFFIX_OVERLAY = "-overlay"
FILENAME_SUFFIX_TRACE = "-trace"

_ext_to_file_type = {'.txt': 'text', '.png': 'image', '.wav': 'audio'}

# Matches new failures in TestHarness.js tests.
TESTHARNESS_JS_FAILURE_RE = re.compile(r'\+(?:FAIL|Harness Error\.) (.*)$')

# Matches fatal log lines. In Chrome, these trigger process crash.
# Such lines typically written as part of a (D)Check failures, but there
# are other types of FATAL messages too.
# The first capture group captures the name of the file which generated
# the fatal message, the second captures the message itself.
FATAL_MESSAGE_RE = re.compile(
    r'^.*FATAL.*?([a-zA-Z0-9_.]+\.[a-zA-Z0-9_]+\([0-9]+\))\]? (.*)$',
    re.MULTILINE)

IGNORE_RESULT = 'IGNORE'


def has_failure_type(failure_type, failure_list):
    return any(isinstance(failure, failure_type) for failure in failure_list)


class AbstractTestResultType(object):
    port = None
    test_name = None
    filesystem = None
    result_directory = None
    result = ResultType.Pass

    def __init__(self, actual_driver_output, expected_driver_output):
        self.actual_driver_output = actual_driver_output
        self.expected_driver_output = expected_driver_output
        self.has_stderr = False
        self.has_repaint_overlay = False
        self.is_reftest = False
        if actual_driver_output:
            self.has_stderr = actual_driver_output.has_stderr()
        if expected_driver_output:
            self.has_stderr |= expected_driver_output.has_stderr()

    def _artifact_is_text(self, path):
        ext = self.filesystem.splitext(path)[1]
        return ext != '.png' and ext != '.wav'

    def _write_to_artifacts(self, typ_artifacts, artifact_name, path, content,
                            force_overwrite):
        typ_artifacts.CreateArtifact(
            artifact_name, path, content, force_overwrite=force_overwrite)

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        typ_artifacts_dir = self.filesystem.join(
            self.result_directory, typ_artifacts.ArtifactsSubDirectory())
        if self.actual_driver_output.command:
            artifact_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_CMD, '.txt')
            artifacts_abspath = self.filesystem.join(typ_artifacts_dir,
                                                     artifact_filename)
            if (force_overwrite or self.result != ResultType.Pass
                    or not self.filesystem.exists(artifacts_abspath)):
                self._write_to_artifacts(typ_artifacts,
                                         'command',
                                         artifact_filename,
                                         self.actual_driver_output.command,
                                         force_overwrite=True)

        if self.actual_driver_output.error:
            artifact_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_STDERR, '.txt')
            artifacts_abspath = self.filesystem.join(typ_artifacts_dir,
                                                     artifact_filename)
            # If a test has multiple stderr results, keep that of the last
            # failure, which is useful for debugging flaky tests with
            # --iterations=n or --repeat-each=n.
            if (force_overwrite or self.result != ResultType.Pass
                    or not self.filesystem.exists(artifacts_abspath)):
                self._write_to_artifacts(typ_artifacts,
                                         'stderr',
                                         artifact_filename,
                                         self.actual_driver_output.error,
                                         force_overwrite=True)

    def _error_text(self):
        if (self.actual_driver_output
                and self.actual_driver_output.error is not None):
            # Even when running under py3, some clients pass a str
            # to error instead of bytes. We must handle both.
            if (six.PY3
                    and not isinstance(self.actual_driver_output.error, str)):
                return self.actual_driver_output.error.decode(
                    'utf8', 'replace')
            else:
                return self.actual_driver_output.error
        return ''

    @staticmethod
    def loads(s):
        """Creates a AbstractTestResultType object from the specified string."""
        return cPickle.loads(s)

    def message(self):
        """Returns a string describing the failure in more detail."""
        raise NotImplementedError

    def failure_reason(self):
        """Returns a FailureReason object describing why the test failed, if
        the test failed and suitable reasons are available.
        The information is used in LUCI result viewing UIs and will be
        used to cluster similar failures together."""

        error_text = self._error_text()
        match = FATAL_MESSAGE_RE.search(error_text)
        if match:
            # The file name and line number, e.g. "my_file.cc(123)".
            file_name = match.group(1)
            # The log message, e.g. "Check failed: task_queue_.IsEmpty()".
            message = match.group(2)

            # Add the file name (and line number) as context to the log
            # message, as some messages are not very specific
            # ("e.g. Check failed: false") and this results in better
            # clustering. It also helps the developer locate the problematic
            # line.
            primary_error = '{}: {}'.format(file_name, message.strip())
            return FailureReason(primary_error)

        return None

    def __eq__(self, other):
        return self.__class__.__name__ == other.__class__.__name__

    def __ne__(self, other):
        return self.__class__.__name__ != other.__class__.__name__

    def __hash__(self):
        return hash(self.__class__.__name__)

    def dumps(self):
        """Returns the string/JSON representation of a AbstractTestResultType."""
        return cPickle.dumps(self)

    def driver_needs_restart(self):
        """Returns True if we should kill the driver before the next test."""
        return False

    def text_mismatch_category(self):
        raise NotImplementedError


class PassWithStderr(AbstractTestResultType):
    def __init__(self, driver_output):
        # TODO (weizhong): Should we write out the reference driver standard
        # error
        super(PassWithStderr, self).__init__(driver_output, None)

    def message(self):
        return 'test passed but has standard error output'


class TestFailure(AbstractTestResultType):
    result = ResultType.Failure


class FailureTimeout(AbstractTestResultType):
    result = ResultType.Timeout

    def __init__(self, actual_driver_output, is_reftest=False):
        super(FailureTimeout, self).__init__(actual_driver_output, None)
        self.is_reftest = is_reftest

    def message(self):
        if self.is_reftest:
            return 'test reference timed out'
        else:
            return 'test timed out'

    def driver_needs_restart(self):
        return True


class FailureCrash(AbstractTestResultType):
    result = ResultType.Crash

    def __init__(self,
                 actual_driver_output,
                 is_reftest=False,
                 process_name='content_shell',
                 pid=None,
                 has_log=False):
        super(FailureCrash, self).__init__(actual_driver_output, None)
        self.process_name = process_name
        self.pid = pid
        self.is_reftest = is_reftest
        self.has_log = has_log
        self.crash_log = self.actual_driver_output.crash_log

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureCrash, self).create_artifacts(typ_artifacts,
                                                   force_overwrite)
        if self.crash_log:
            artifact_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_CRASH_LOG, '.txt')
            self._write_to_artifacts(typ_artifacts, 'crash_log',
                                     artifact_filename, self.crash_log,
                                     force_overwrite)

    def message(self):
        if self.pid:
            return self._pid_message_format() % (self.process_name, self.pid)
        else:
            return self._process_message_format() % (self.process_name)

    def _pid_message_format(self):
        return self._process_message_format() + ' [pid=%d]'

    def _process_message_format(self):
        if self.is_reftest:
            return '%s crashed in reference'
        else:
            return '%s crashed'

    def driver_needs_restart(self):
        return True


class FailureLeak(TestFailure):
    def __init__(self, actual_driver_output, is_reftest=False):
        super(FailureLeak, self).__init__(actual_driver_output, None)
        self.is_reftest = is_reftest

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureLeak, self).create_artifacts(typ_artifacts,
                                                  force_overwrite)
        artifact_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_LEAK_LOG, '.txt')
        self.log = self.actual_driver_output.leak_log
        self._write_to_artifacts(typ_artifacts, 'leak_log', artifact_filename,
                                 self.log, force_overwrite)

    def message(self):
        return self._message_format() % (self.log)

    def _message_format(self):
        if self.is_reftest:
            return 'leak detected in reference: %s'
        else:
            return 'leak detected: %s'


class ActualAndBaselineArtifacts(TestFailure):
    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(ActualAndBaselineArtifacts, self).create_artifacts(
            typ_artifacts, force_overwrite)
        self.actual_artifact_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_ACTUAL, self.file_ext)
        self.expected_artifact_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_EXPECTED, self.file_ext)
        attr = _ext_to_file_type[self.file_ext]
        if getattr(self.actual_driver_output, attr):
            self._write_to_artifacts(typ_artifacts, 'actual_%s' % attr,
                                     self.actual_artifact_filename,
                                     getattr(self.actual_driver_output,
                                             attr), force_overwrite)
        if getattr(self.expected_driver_output, attr):
            self._write_to_artifacts(
                typ_artifacts, 'expected_%s' % attr,
                self.expected_artifact_filename,
                getattr(self.expected_driver_output, attr), force_overwrite)

    def message(self):
        raise NotImplementedError


class FailureText(ActualAndBaselineArtifacts):
    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureText, self).__init__(actual_driver_output,
                                          expected_driver_output)
        self.has_repaint_overlay = (
            repaint_overlay.result_contains_repaint_rects(
                self._actual_text())
            or repaint_overlay.result_contains_repaint_rects(
                self._expected_text()))
        self.file_ext = '.txt'

    def _actual_text(self):
        if (self.actual_driver_output
                and self.actual_driver_output.text is not None):
            if six.PY3:
                # TODO(crbug/1197331): We should not decode here looks like.
                # html_diff expects it to be bytes for comparing to account
                # various types of encodings.
                # html_diff.py and unified_diff.py use str types during
                # diff fixup. Will handle it later.
                return self.actual_driver_output.text.decode('utf8', 'replace')
            else:
                return self.actual_driver_output.text
        return ''

    def _expected_text(self):
        if (self.expected_driver_output
                and self.expected_driver_output.text is not None):
            if six.PY3:
                # TODO(crbug/1197331): We should not decode here looks like.
                # html_diff expects it to be bytes for comparing to account
                # various types of encodings.
                # html_diff.py and unified_diff.py use str types during
                # diff fixup. Will handle it later.
                return self.expected_driver_output.text.decode(
                    'utf8', 'replace')
            else:
                return self.expected_driver_output.text
        return ''

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        # TODO (weizhong): See if you can can only output diff files for
        # non empty text.
        super(FailureText, self).create_artifacts(typ_artifacts,
                                                  force_overwrite)

        actual_text = self._actual_text()
        expected_text = self._expected_text()

        artifacts_abs_path = self.filesystem.join(
            self.result_directory, typ_artifacts.ArtifactsSubDirectory())
        diff_content = unified_diff(
            expected_text, actual_text,
            self.filesystem.join(artifacts_abs_path,
                                 self.expected_artifact_filename),
            self.filesystem.join(artifacts_abs_path,
                                 self.actual_artifact_filename))
        diff_filename = self.port.output_filename(self.test_name,
                                                  FILENAME_SUFFIX_DIFF, '.txt')
        html_diff_content = html_diff(expected_text, actual_text)
        html_diff_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_HTML_DIFF, '.html')

        # TODO(crbug/1197331): Revisit while handling the diff modules.
        if diff_content and six.PY3:
            diff_content = diff_content.encode('utf8', 'replace')
        if html_diff_content and six.PY3:
            html_diff_content = html_diff_content.encode('utf8', 'replace')

        self._write_to_artifacts(typ_artifacts, 'text_diff', diff_filename,
                                 diff_content, force_overwrite)
        self._write_to_artifacts(typ_artifacts, 'pretty_text_diff',
                                 html_diff_filename, html_diff_content,
                                 force_overwrite)

    def message(self):
        raise NotImplementedError

    def text_mismatch_category(self):
        raise NotImplementedError

    def failure_reason(self):
        actual_text = self._actual_text()
        expected_text = self._expected_text()
        diff_content = unified_diff(expected_text, actual_text, "expected",
                                    "actual")
        diff_lines = diff_content.splitlines()

        # Drop the standard diff header with the following lines:
        # --- expected
        # +++ actual
        diff_lines = diff_lines[2:]

        # Find the first block of additions and/or deletions in the diff.
        # E.g.
        #  Unchanged line
        # -Old line 1  <-- Start of block
        # -Old line 2
        # +New line 1
        # +New line 2  <-- End of block
        #  Unchanged line
        deleted_lines = []
        added_lines = []
        match_state = ''
        for i, line in enumerate(diff_lines):
            # A block of diffs starts with removals (-)
            # and then additions (+). Any variation from this
            # pattern indicates an end of this block of diffs.
            if ((line.startswith(' ') and match_state != '')
                    or (line.startswith('-') and match_state == '+')):
                # End of block of additions and deletions.
                break
            if line.startswith('-'):
                match_state = '-'
                deleted_lines.append(line)
            if line.startswith('+'):
                match_state = '+'
                added_lines.append(line)

        primary_error = None

        if 'This is a testharness.js-based test.' in actual_text:
            # Testharness.js test. Find the first new failure (if any) and
            # report it as the failure reason.
            for i, line in enumerate(added_lines):
                match = TESTHARNESS_JS_FAILURE_RE.match(line)
                if match:
                    primary_error = match.group(1)
                    break
        else:
            # Reconstitute the first diff block, but put added lines before
            # the deleted lines as they are usually more interesting.
            # (New actual output more interesting than missing expected
            # output, as it is likely to contain errors.)
            first_diff_block = '\n'.join(added_lines + deleted_lines)

            if len(first_diff_block) >= 30:
                # Only use the diff if it is not tiny. If it is only the
                # addition of an empty line at the end of the file or
                # similar, it is unlikely to be useful.
                primary_error = ('Unexpected Diff (+got, -want):\n' +
                                 first_diff_block)

        if primary_error:
            return FailureReason(primary_error)
        return None


class FailureMissingResult(FailureText):
    def message(self):
        return '-expected.txt was missing'


class FailureTextNotGenerated(FailureText):
    def message(self):
        return 'test did not generate text results'


class FailureTextMismatch(FailureText):
    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureTextMismatch, self).create_artifacts(
            typ_artifacts, force_overwrite)
        if six.PY2:
            html = repaint_overlay.generate_repaint_overlay_html(
                self.test_name, self.actual_driver_output.text,
                self.expected_driver_output.text)
        else:
            html = repaint_overlay.generate_repaint_overlay_html(
                self.test_name,
                self.actual_driver_output.text.decode('utf8', 'replace'),
                self.expected_driver_output.text.decode('utf8', 'replace'))
        if html:
            overlay_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_OVERLAY, '.html')
            self._write_to_artifacts(typ_artifacts, 'overlay',
                                     overlay_filename,
                                     html.encode('utf8',
                                                 'replace'), force_overwrite)

    def message(self):
        return 'text diff'

    def text_mismatch_category(self):
        return 'general text mismatch'


class FailureTestHarnessAssertion(FailureText):
    def message(self):
        return 'asserts failed'


class FailureSpacesAndTabsTextMismatch(FailureTextMismatch):
    def message(self):
        return 'text diff by spaces and tabs only'

    def text_mismatch_category(self):
        return 'spaces and tabs only'


class FailureLineBreaksTextMismatch(FailureTextMismatch):
    def message(self):
        return 'text diff by newlines only'

    def text_mismatch_category(self):
        return 'newlines only'


class FailureSpaceTabLineBreakTextMismatch(FailureTextMismatch):
    def message(self):
        return 'text diff by spaces, tabs and newlines only'

    def text_mismatch_category(self):
        return 'spaces, tabs and newlines only'


class FailureImage(ActualAndBaselineArtifacts):
    # Tag key used to report the actual image's hash to ResultDB.
    ACTUAL_HASH_RDB_TAG: ClassVar[str] = 'web_tests_actual_image_hash'

    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureImage, self).__init__(actual_driver_output,
                                           expected_driver_output)
        self.file_ext = '.png'

    def message(self):
        raise NotImplementedError


class FailureImageHashNotGenerated(FailureImage):
    def message(self):
        return 'test did not generate image results'


class FailureMissingImageHash(FailureImage):
    def message(self):
        return '-expected.png was missing an embedded checksum'


class FailureMissingImage(FailureImage):
    def message(self):
        return '-expected.png was missing'


class FailureImageHashMismatch(FailureImage):
    def message(self):
        return 'image diff'

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        # Need this if statement in case the image diff process fails
        if self.actual_driver_output.image_diff:
            diff_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_DIFF, '.png')
            diff = self.actual_driver_output.image_diff
            self._write_to_artifacts(typ_artifacts, 'image_diff',
                                     diff_filename, diff, force_overwrite)

        super(FailureImageHashMismatch, self).create_artifacts(
            typ_artifacts, force_overwrite)


class FailureReftestMixin(object):
    # This mixin will be used by reftest failure types to create a reference
    # file artifact along with image mismatch artifacts, actual image output,
    # reference driver output, and standard error output. The actual reftest
    # failure types used in single_test_runner.py each have an inheritance list.
    # The order of this list decides the call order of overridden methods like the
    # constructor and create_artifacts. For example, FailureReftestMismatch
    # has FailureReftestMixin followed by FailureImageHashMismatch. So when
    # create_artifacts is called on that class, FailureReftestMixin's create_artifacts
    # will be called first and then when that method calls the super class's
    # create_artifacts, it will call FailureImageHashMismatch's create_artifacts.

    def __init__(self,
                 actual_driver_output,
                 expected_driver_output,
                 reference_filename=None):
        super(FailureReftestMixin, self).__init__(actual_driver_output,
                                                  expected_driver_output)
        self.reference_filename = reference_filename
        self.reference_file_type = 'reference_file_mismatch'

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureReftestMixin, self).create_artifacts(
            typ_artifacts, force_overwrite)
        sub_dir = typ_artifacts.ArtifactsSubDirectory()
        artifact_filename = self.filesystem.join(
            sub_dir, self.filesystem.dirname(self.test_name),
            self.filesystem.basename(self.reference_filename))
        artifact_abspath = self.filesystem.join(self.result_directory,
                                                artifact_filename)
        # a reference test may include a page that does not exist in the
        # web test directory, like about:blank pages
        if (not self.filesystem.exists(artifact_abspath)
                and self.filesystem.exists(self.reference_filename)):
            self.filesystem.maybe_make_directory(
                self.filesystem.dirname(artifact_abspath))
            self.filesystem.copyfile(self.reference_filename, artifact_abspath)
        typ_artifacts.AddArtifact(
            self.reference_file_type,
            artifact_filename,
            raise_exception_for_duplicates=False)

    def message(self):
        raise NotImplementedError


class FailureReftestMismatch(FailureReftestMixin, FailureImageHashMismatch):
    def message(self):
        return 'reference mismatch'


class FailureReftestMismatchDidNotOccur(FailureReftestMixin, FailureImage):
    def __init__(self,
                 actual_driver_output,
                 expected_driver_output,
                 reference_filename=None):
        super(FailureReftestMismatchDidNotOccur, self).__init__(
            actual_driver_output, expected_driver_output, reference_filename)
        self.reference_file_type = 'reference_file_match'

    def message(self):
        return "reference mismatch didn't happen"


class FailureReftestNoImageGenerated(FailureReftestMixin, FailureImage):
    def message(self):
        return "reference test didn't generate pixel results"


class FailureReftestNoReferenceImageGenerated(FailureReftestMixin,
                                              FailureImage):
    def message(self):
        return "-expected.html didn't generate pixel results"


class FailureAudio(ActualAndBaselineArtifacts):
    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureAudio, self).__init__(actual_driver_output,
                                           expected_driver_output)
        self.file_ext = '.wav'

    def message(self):
        raise NotImplementedError


class FailureMissingAudio(FailureAudio):
    def message(self):
        return 'expected audio result was missing'


class FailureAudioMismatch(FailureAudio):
    def message(self):
        return 'audio mismatch'


class FailureAudioNotGenerated(FailureAudio):
    def message(self):
        return 'audio result not generated'


class FailureEarlyExit(AbstractTestResultType):
    result = ResultType.Skip

    def __init__(self, actual_driver_output=None, expected_driver_output=None):
        super(FailureEarlyExit, self).__init__(actual_driver_output,
                                               expected_driver_output)

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        pass

    def message(self):
        return 'skipped due to early exit'


class TraceFileArtifact(AbstractTestResultType):
    result = IGNORE_RESULT

    def __init__(self, actual_driver_output, trace_file, suffix):
        super(TraceFileArtifact, self).__init__(actual_driver_output, None)
        self._trace_file = trace_file
        self._suffix = suffix

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        typ_artifacts_dir = self.filesystem.join(
            self.result_directory, typ_artifacts.ArtifactsSubDirectory())
        trace_file = self._trace_file
        if (trace_file and self.filesystem.exists(trace_file)):
            artifact_filename = self.port.output_filename(
                self.test_name, self._suffix, '.pftrace')
            artifacts_abspath = self.filesystem.join(typ_artifacts_dir,
                                                     artifact_filename)
            if (force_overwrite
                    or not self.filesystem.exists(artifacts_abspath)):
                with self.filesystem.open_binary_file_for_reading(
                        trace_file) as trace_fh:
                    typ_artifacts.CreateArtifact(
                        'trace',
                        artifact_filename,
                        trace_fh.read(),
                        force_overwrite=force_overwrite)

    def message(self):
        return 'test produced a trace file'


# Convenient collection of all failure classes for anything that might
# need to enumerate over them all.
ALL_FAILURE_CLASSES = (FailureTimeout, FailureCrash, FailureMissingResult,
                       FailureTestHarnessAssertion, FailureTextMismatch,
                       FailureSpacesAndTabsTextMismatch,
                       FailureLineBreaksTextMismatch,
                       FailureSpaceTabLineBreakTextMismatch,
                       FailureMissingImageHash, FailureMissingImage,
                       FailureImageHashMismatch, FailureReftestMismatch,
                       FailureReftestMismatchDidNotOccur,
                       FailureReftestNoImageGenerated,
                       FailureReftestNoReferenceImageGenerated,
                       FailureMissingAudio, FailureAudioMismatch,
                       FailureEarlyExit, FailureImageHashNotGenerated,
                       FailureTextNotGenerated, FailureAudioNotGenerated)

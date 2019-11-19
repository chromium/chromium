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

import cPickle

from blinkpy.web_tests.models import test_expectations
from blinkpy.web_tests.controllers import repaint_overlay
from blinkpy.common.html_diff import html_diff
from blinkpy.common.unified_diff import unified_diff

# TODO(rmhasan) Create a unit test for each Failure type and make
# sure each artifact is written to the correct path

# FIXME: old-run-webkit-tests shows the diff percentage as the text
#   contents of the "diff" link.
# FIXME: old-run-webkit-tests include a link to the test file.
_image_diff_html_template = """<!DOCTYPE HTML>
<html>
<head>
<title>%(title)s</title>
<style>.label{font-weight:bold}</style>
</head>
<body>
Difference between images: <a href="%(diff_filename)s">diff</a><br>
<div class=imageText></div>
<div class=imageContainer data-prefix="%(prefix)s">Loading...</div>
<script>
(function() {
var preloadedImageCount = 0;
function preloadComplete() {
++preloadedImageCount;
if (preloadedImageCount < 2)
    return;
toggleImages();
setInterval(toggleImages, 2000)
}

function preloadImage(url) {
image = new Image();
image.addEventListener('load', preloadComplete);
image.src = url;
return image;
}

function toggleImages() {
if (text.textContent == 'Expected Image') {
    text.textContent = 'Actual Image';
    container.replaceChild(actualImage, container.firstChild);
} else {
    text.textContent = 'Expected Image';
    container.replaceChild(expectedImage, container.firstChild);
}
}

var text = document.querySelector('.imageText');
var container = document.querySelector('.imageContainer');
var actualImage = preloadImage(container.getAttribute('data-prefix') + '-actual.png');
var expectedImage = preloadImage(container.getAttribute('data-prefix') + '-expected.png');
})();
</script>
</body>
</html>"""

# Filename pieces when writing failures to the test results directory.
FILENAME_SUFFIX_ACTUAL = "-actual"
FILENAME_SUFFIX_EXPECTED = "-expected"
FILENAME_SUFFIX_DIFF = "-diff"
FILENAME_SUFFIX_DIFFS = "-diffs"
FILENAME_SUFFIX_STDERR = "-stderr"
FILENAME_SUFFIX_CRASH_LOG = "-crash-log"
FILENAME_SUFFIX_SAMPLE = "-sample"
FILENAME_SUFFIX_LEAK_LOG = "-leak-log"
FILENAME_SUFFIX_HTML_DIFF = "-pretty-diff"
FILENAME_SUFFIX_OVERLAY = "-overlay"


_ext_to_file_type = {
    '.txt': 'text', '.png': 'image', '.wav': 'audio'}


def has_failure_type(failure_type, failure_list):
    return any(isinstance(failure, failure_type) for failure in failure_list)


class AbstractTestResultType(object):
    port = None
    test_name = None
    filesystem = None
    result_directory = None
    result = test_expectations.PASS

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

    def _write_to_artifacts(
            self, typ_artifacts, artifact_name, path, content, force_overwrite):
        typ_artifacts.CreateArtifact(
            artifact_name, path, content, force_overwrite=force_overwrite)

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        if self.actual_driver_output.error:
            artifact_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_STDERR, '.txt')

            # some ref tests don't produce any text output and also
            # have a text baseline. They also produce an image mismatch
            # error. If the test driver produces stderr then an exception
            # will be raised because we will be writing that stderr twice
            artifacts_abspath = self.filesystem.join(
                self.result_directory, typ_artifacts.ArtifactsSubDirectory(),
                artifact_filename)
            if not self.filesystem.exists(artifacts_abspath):
                self._write_to_artifacts(
                    typ_artifacts, 'stderr', artifact_filename,
                    self.actual_driver_output.error, force_overwrite=True)

    @staticmethod
    def loads(s):
        """Creates a AbstractTestResultType object from the specified string."""
        return cPickle.loads(s)

    def message(self):
        """Returns a string describing the failure in more detail."""
        raise NotImplementedError

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

    def message(self):
        raise NotImplementedError

    def text_mismatch_category(self):
        raise NotImplementedError


class PassWithStderr(AbstractTestResultType):

    def __init__(self, driver_output):
        # TODO (rmhasan): Should we write out the reference driver standard
        # error
        super(PassWithStderr, self).__init__(driver_output, None)

    def message(self):
        return 'test passed but has standard error output'


class TestFailure(AbstractTestResultType):
    result = test_expectations.FAIL


class FailureTimeout(AbstractTestResultType):
    result = test_expectations.TIMEOUT

    def __init__(self, actual_driver_output, is_reftest=False):
        super(FailureTimeout, self).__init__(
            actual_driver_output, None)
        self.is_reftest = is_reftest

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        pass

    def message(self):
        return 'test timed out'

    def driver_needs_restart(self):
        return True


class FailureCrash(AbstractTestResultType):
    result = test_expectations.CRASH

    def __init__(self, actual_driver_output, is_reftest=False,
                 process_name='content_shell', pid=None, has_log=False):
        super(FailureCrash, self).__init__(
            actual_driver_output, None)
        self.process_name = process_name
        self.pid = pid
        self.is_reftest = is_reftest
        self.has_log = has_log
        self.crash_log = self.actual_driver_output.crash_log

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureCrash, self).create_artifacts(typ_artifacts, force_overwrite)
        if self.crash_log:
            artifact_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_CRASH_LOG, '.txt')
            self._write_to_artifacts(
                typ_artifacts, 'crash_log', artifact_filename,
                self.crash_log.encode('utf8', 'replace'), force_overwrite)

    def message(self):
        if self.pid:
            return '%s crashed [pid=%d]' % (self.process_name, self.pid)
        return self.process_name + ' crashed'

    def driver_needs_restart(self):
        return True


class FailureLeak(TestFailure):

    def __init__(self, actual_driver_output, is_reftest=False):
        super(FailureLeak, self).__init__(
            actual_driver_output, None)
        self.is_reftest = is_reftest

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureLeak, self).create_artifacts(typ_artifacts, force_overwrite)
        artifact_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_LEAK_LOG, '.txt')
        self.log = self.actual_driver_output.leak_log
        self._write_to_artifacts(
            typ_artifacts, 'leak_log', artifact_filename, self.log, force_overwrite)

    def message(self):
        return 'leak detected: %s' % (self.log)


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
            self._write_to_artifacts(
                typ_artifacts, 'actual_%s' % attr, self.actual_artifact_filename,
                getattr(self.actual_driver_output, attr), force_overwrite)
        if getattr(self.expected_driver_output, attr):
            self._write_to_artifacts(
                typ_artifacts, 'expected_%s' % attr, self.expected_artifact_filename,
                getattr(self.expected_driver_output, attr), force_overwrite)

    def message(self):
        raise NotImplementedError


class FailureText(ActualAndBaselineArtifacts):

    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureText, self).__init__(
            actual_driver_output, expected_driver_output)
        self.has_repaint_overlay = (
            repaint_overlay.result_contains_repaint_rects(
                actual_driver_output.text) or
            repaint_overlay.result_contains_repaint_rects(
                expected_driver_output.text))
        self.file_ext = '.txt'

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        # TODO (rmhasan): See if you can can only output diff files for
        # non empty text.
        super(FailureText, self).create_artifacts(
            typ_artifacts, force_overwrite)
        expected_text = self.expected_driver_output.text or ''
        actual_text = self.actual_driver_output.text or ''
        artifacts_abs_path = self.filesystem.join(
            self.result_directory, typ_artifacts.ArtifactsSubDirectory())
        diff_content = unified_diff(
          expected_text, actual_text,
          self.filesystem.join(artifacts_abs_path, self.expected_artifact_filename),
          self.filesystem.join(artifacts_abs_path, self.actual_artifact_filename))
        diff_filename = self.port.output_filename(
          self.test_name, FILENAME_SUFFIX_DIFF, '.txt')
        html_diff_content = html_diff(expected_text, actual_text)
        html_diff_filename = self.port.output_filename(
            self.test_name, FILENAME_SUFFIX_HTML_DIFF, '.html')
        self._write_to_artifacts(
            typ_artifacts, 'text_diff', diff_filename, diff_content, force_overwrite)
        self._write_to_artifacts(
            typ_artifacts, 'pretty_text_diff', html_diff_filename,
            html_diff_content, force_overwrite)

    def message(self):
        raise NotImplementedError

    def text_mismatch_category(self):
        raise NotImplementedError


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
        html = repaint_overlay.generate_repaint_overlay_html(
            self.test_name, self.actual_driver_output.text,
            self.expected_driver_output.text)
        if html:
            overlay_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_OVERLAY, '.html')
            self._write_to_artifacts(
                typ_artifacts, 'overlay', overlay_filename, html, force_overwrite)

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

    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureImage, self).__init__(
            actual_driver_output, expected_driver_output)
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
            self._write_to_artifacts(
                typ_artifacts, 'image_diff', diff_filename, diff, force_overwrite)
            diffs_html_filename = self.port.output_filename(
                self.test_name, FILENAME_SUFFIX_DIFFS, '.html')
            diffs_html = _image_diff_html_template % {
                'title': self.test_name, 'diff_filename': diff_filename,
                'prefix': self.port.output_filename(self.test_name, '', '')}
            self._write_to_artifacts(
                typ_artifacts, 'pretty_image_diff', diffs_html_filename,
                diffs_html, force_overwrite)

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

    def __init__(self, actual_driver_output, expected_driver_output,
                 reference_filename=None):
        super(FailureReftestMixin, self).__init__(
            actual_driver_output, expected_driver_output)
        self.reference_filename = reference_filename
        self.reference_file_type = 'reference_file_mismatch'

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        super(FailureReftestMixin, self).create_artifacts(
            typ_artifacts, force_overwrite)
        sub_dir = typ_artifacts.ArtifactsSubDirectory()
        artifact_filename = self.filesystem.join(
            sub_dir, self.filesystem.dirname(self.test_name),
            self.filesystem.basename(self.reference_filename))
        artifact_abspath = self.filesystem.join(
            self.result_directory, artifact_filename)
        # a reference test may include a page that does not exist in the
        # web test directory, like about:blank pages
        if (not self.filesystem.exists(artifact_abspath) and
                self.filesystem.exists(self.reference_filename)):
            self.filesystem.maybe_make_directory(
                self.filesystem.dirname(artifact_abspath))
            self.filesystem.copyfile(self.reference_filename, artifact_abspath)
        typ_artifacts.AddArtifact(self.reference_file_type, artifact_filename,
                                  raise_exception_for_duplicates=False)

    def message(self):
        raise NotImplementedError


class FailureReftestMismatch(FailureReftestMixin, FailureImageHashMismatch):

    def message(self):
        return 'reference mismatch'


class FailureReftestMismatchDidNotOccur(FailureReftestMixin, FailureImage):

    def __init__(self, actual_driver_output, expected_driver_output,
                 reference_filename=None):
        super(FailureReftestMismatchDidNotOccur, self).__init__(
            actual_driver_output, expected_driver_output, reference_filename)
        self.reference_file_type = 'reference_file_match'

    def message(self):
        return "reference mismatch didn't happen"


class FailureReftestNoImageGenerated(FailureReftestMixin, FailureImage):

    def message(self):
        return "reference test didn't generate pixel results"


class FailureReftestNoReferenceImageGenerated(FailureReftestMixin, FailureImage):

    def message(self):
        return "-expected.html didn't generate pixel results"


class FailureAudio(ActualAndBaselineArtifacts):

    def __init__(self, actual_driver_output, expected_driver_output):
        super(FailureAudio, self).__init__(
            actual_driver_output, expected_driver_output)
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
    result = test_expectations.SKIP

    def __init__(self, actual_driver_output=None, expected_driver_output=None):
        super(FailureEarlyExit, self).__init__(
            actual_driver_output, expected_driver_output)

    def create_artifacts(self, typ_artifacts, force_overwrite=False):
        pass

    def message(self):
        return 'skipped due to early exit'


# Convenient collection of all failure classes for anything that might
# need to enumerate over them all.
ALL_FAILURE_CLASSES = (FailureTimeout, FailureCrash, FailureMissingResult,
                       FailureTestHarnessAssertion,
                       FailureTextMismatch, FailureSpacesAndTabsTextMismatch,
                       FailureLineBreaksTextMismatch, FailureSpaceTabLineBreakTextMismatch,
                       FailureMissingImageHash,
                       FailureMissingImage, FailureImageHashMismatch,
                       FailureReftestMismatch,
                       FailureReftestMismatchDidNotOccur,
                       FailureReftestNoImageGenerated,
                       FailureReftestNoReferenceImageGenerated,
                       FailureMissingAudio, FailureAudioMismatch,
                       FailureEarlyExit, FailureImageHashNotGenerated,
                       FailureTextNotGenerated, FailureAudioNotGenerated)

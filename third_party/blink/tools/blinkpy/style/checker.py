# Copyright (C) 2009 Google Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (chris.jerdonek@gmail.com)
# Copyright (C) 2010 ProFUSION embedded systems
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
"""Front end of some style-checker modules."""

import logging
import os.path
import re
import sys

from blinkpy.common.system.log_utils import configure_logging as _configure_logging
from blinkpy.style.checkers.common import CarriageReturnChecker
from blinkpy.style.checkers.common import categories as CommonCategories
from blinkpy.style.checkers.cpp import CppChecker
from blinkpy.style.checkers.jsonchecker import JSONChecker
from blinkpy.style.checkers.png import PNGChecker
from blinkpy.style.checkers.text import TextChecker
from blinkpy.style.checkers.xml import XMLChecker
from blinkpy.style.error_handlers import DefaultStyleErrorHandler
from blinkpy.style.filter import FilterConfiguration
from blinkpy.style.optparser import ArgumentParser
from blinkpy.style.optparser import DefaultCommandOptionValues

_log = logging.getLogger(__name__)

# These are default option values for the command-line option parser.
_DEFAULT_MIN_CONFIDENCE = 1
_DEFAULT_OUTPUT_FORMAT = 'emacs'

# FIXME: For style categories we will never want to have, remove them.
#        For categories for which we want to have similar functionality,
#        modify the implementation and enable them.
#
# Throughout this module, we use "filter rule" rather than "filter"
# for an individual boolean filter flag like "+foo".  This allows us to
# reserve "filter" for what one gets by collectively applying all of
# the filter rules.
#
# The base filter rules are the filter rules that begin the list of
# filter rules used to check style.  For example, these rules precede
# any user-specified filter rules.  Since by default all categories are
# checked, this list should normally include only rules that begin
# with a "-" sign.
_BASE_FILTER_RULES = [
    '-build/include_what_you_use',  # <string> for std::string
    '-legal/copyright',
    '-readability/fn_size',
    '-readability/casting',
    '-runtime/arrays',  # variable length array
    '-runtime/casting',
    '-runtime/sizeof',
    '-runtime/explicit',  # explicit
    '-runtime/virtual',  # virtual dtor
    '-runtime/printf',
    '-runtime/threadsafe_fn',

    # FIXME: Move the pylint rules from the pylintrc to here. This will
    # also require us to re-work lint_blinkpy.py to produce the equivalent
    # output.
]

# The path-specific filter rules.
#
# This list is order sensitive.  Only the first path substring match
# is used.  See the FilterConfiguration documentation in filter.py
# for more information on this list.
#
# Each string appearing in this nested list should have at least
# one associated unit test assertion.  These assertions are located,
# for example, in the test_path_rules_specifier() unit test method of
# checker_unittest.py.
_PATH_RULES_SPECIFIER = [
    (
        [  # IDL compiler reference output
            # Conforming to style significantly increases the complexity of the code
            # generator and decreases *its* readability, which is of more concern
            # than style of the machine-generated code itself.
            'renderer/bindings/tests/results'
        ],
        ['-']),
    (
        [  # Due to historical reasons scheduler uses Chromium style instead of
            # Blink style.
            'renderer/platform/scheduler',
            'public/platform/scheduler'
        ],
        ['-readability/control_flow']),

    # perf_tests contains a lot of third-party files.
    (['perf_tests/'], ['-whitespace/tab'])
]

_CPP_FILE_EXTENSIONS = [
    'c',
    'cc',
    'cpp',
    'h',
]

_JSON_FILE_EXTENSION = 'json'

_TEXT_FILE_EXTENSIONS = [
    'cgi',
    'css',
    'gyp',
    'gypi',
    'html',
    'idl',
    'in',
    'js',
    'mm',
    'php',
    'pl',
    'pm',
    'rb',
    'sh',
    'txt',
    'xhtml',
    'y',
]

_XML_FILE_EXTENSIONS = [
    'vcproj',
    'vsprops',
]

_PNG_FILE_EXTENSION = 'png'

# Files to skip that are more common or obvious.
#
# This list should be in addition to files with FileType.NONE.  Files
# with FileType.NONE are automatically skipped without warning.
_SKIPPED_FILES_WITHOUT_WARNING = [
    'web_tests' + os.path.sep,
    'third_party' + os.path.sep + 'blink' + os.path.sep + 'renderer' +
    os.path.sep + 'devtools' + os.path.sep + 'protocol.json',
]

# Extensions of files which are allowed to contain carriage returns.
_CARRIAGE_RETURN_ALLOWED_FILE_EXTENSIONS = [
    'png',
    'vcproj',
    'vsprops',
]

# The maximum number of errors to report per file, per category.
# If a category is not a key, then it has no maximum.
_MAX_REPORTS_PER_CATEGORY = {'whitespace/carriage_return': 1}


def _all_categories():
    """Return the set of all categories used by check_blink_style.py."""
    # Take the union across all checkers.
    categories = CommonCategories.union(CppChecker.categories)
    categories = categories.union(JSONChecker.categories)
    categories = categories.union(PNGChecker.categories)
    return categories


def _check_blink_style_defaults():
    """Return the default command-line options for check_blink_style.py."""
    return DefaultCommandOptionValues(
        min_confidence=_DEFAULT_MIN_CONFIDENCE,
        output_format=_DEFAULT_OUTPUT_FORMAT)


# This function assists in optparser not having to import from checker.
def check_blink_style_parser():
    all_categories = _all_categories()
    default_options = _check_blink_style_defaults()
    return ArgumentParser(
        all_categories=all_categories,
        base_filter_rules=_BASE_FILTER_RULES,
        default_options=default_options)


def check_blink_style_configuration(options):
    """Return a StyleProcessorConfiguration instance for check_blink_style.py.

    Args:
      options: A CommandOptionValues instance.
    """
    filter_configuration = FilterConfiguration(
        base_rules=_BASE_FILTER_RULES,
        path_specific=_PATH_RULES_SPECIFIER,
        user_rules=options.filter_rules)

    return StyleProcessorConfiguration(
        filter_configuration=filter_configuration,
        max_reports_per_category=_MAX_REPORTS_PER_CATEGORY,
        min_confidence=options.min_confidence,
        output_format=options.output_format,
        stderr_write=sys.stderr.write)


def _create_log_handlers(stream):
    """Create and return a default list of logging.Handler instances.

    Format WARNING messages and above to display the logging level, and
    messages strictly below WARNING not to display it.

    Args:
      stream: See the configure_logging() docstring.
    """
    # Handles logging.WARNING and above.
    error_handler = logging.StreamHandler(stream)
    error_handler.setLevel(logging.WARNING)
    formatter = logging.Formatter('%(levelname)s: %(message)s')
    error_handler.setFormatter(formatter)

    # Create a logging.Filter instance that only accepts messages
    # below WARNING (i.e. filters out anything WARNING or above).
    non_error_filter = logging.Filter()
    # The filter method accepts a logging.LogRecord instance.
    non_error_filter.filter = lambda record: record.levelno < logging.WARNING

    non_error_handler = logging.StreamHandler(stream)
    non_error_handler.addFilter(non_error_filter)
    formatter = logging.Formatter('%(message)s')
    non_error_handler.setFormatter(formatter)

    return [error_handler, non_error_handler]


def _create_debug_log_handlers(stream):
    """Create and return a list of logging.Handler instances for debugging.

    Args:
      stream: See the configure_logging() docstring.
    """
    handler = logging.StreamHandler(stream)
    formatter = logging.Formatter('%(name)s: %(levelname)-8s %(message)s')
    handler.setFormatter(formatter)

    return [handler]


def configure_logging(stream, logger=None, is_verbose=False):
    """Configure logging, and return the list of handlers added.

    Returns:
      A list of references to the logging handlers added to the root
      logger.  This allows the caller to later remove the handlers
      using logger.removeHandler.  This is useful primarily during unit
      testing where the caller may want to configure logging temporarily
      and then undo the configuring.

    Args:
      stream: A file-like object to which to log.  The stream must
              define an "encoding" data attribute, or else logging
              raises an error.
      logger: A logging.logger instance to configure.  This parameter
              should be used only in unit tests.  Defaults to the
              root logger.
      is_verbose: A boolean value of whether logging should be verbose.
    """
    # If the stream does not define an "encoding" data attribute, the
    # logging module can throw an error like the following:
    #
    # Traceback (most recent call last):
    #   File "/System/Library/Frameworks/Python.framework/Versions/2.6/...
    #         lib/python2.6/logging/__init__.py", line 761, in emit
    #     self.stream.write(fs % msg.encode(self.stream.encoding))
    # LookupError: unknown encoding: unknown
    if logger is None:
        logger = logging.getLogger()

    if is_verbose:
        logging_level = logging.DEBUG
        handlers = _create_debug_log_handlers(stream)
    else:
        logging_level = logging.INFO
        handlers = _create_log_handlers(stream)

    handlers = _configure_logging(
        logging_level=logging_level, logger=logger, handlers=handlers)

    return handlers


# Enum-like idiom
class FileType:

    NONE = 0  # FileType.NONE evaluates to False.
    # Alphabetize remaining types
    # CHANGELOG = 1
    CPP = 2
    JSON = 3
    PNG = 4
    TEXT = 6
    # WATCHLIST = 7
    XML = 8


class CheckerDispatcher(object):
    """Supports determining whether and how to check style, based on path."""

    def _file_extension(self, file_path):
        """Return the file extension without the leading dot."""
        return os.path.splitext(file_path)[1].lstrip('.')

    def _should_skip_file_path(self, file_path, skip_array_entry):
        match = re.search(r"\s*png$", file_path)
        if match:
            return False
        if isinstance(skip_array_entry, str):
            if file_path.find(skip_array_entry) >= 0:
                return True
        elif skip_array_entry.match(file_path):
            return True
        return False

    def should_skip_without_warning(self, file_path):
        """Return whether the given file should be skipped without a warning."""
        if not self._file_type(file_path):  # FileType.NONE.
            return True
        for skipped_file in _SKIPPED_FILES_WITHOUT_WARNING:
            if self._should_skip_file_path(file_path, skipped_file):
                return True
        return False

    def should_check_and_strip_carriage_returns(self, file_path):
        return (self._file_extension(file_path) not in
                _CARRIAGE_RETURN_ALLOWED_FILE_EXTENSIONS)

    def _file_type(self, file_path):
        """Return the file type corresponding to the given file."""
        file_extension = self._file_extension(file_path)

        if (file_extension in _CPP_FILE_EXTENSIONS) or (file_path == '-'):
            # FIXME: Do something about the comment below and the issue it
            #        raises since cpp_style already relies on the extension.
            #
            # Treat stdin as C++. Since the extension is unknown when
            # reading from stdin, cpp_style tests should not rely on
            # the extension.
            return FileType.CPP
        elif file_extension == _JSON_FILE_EXTENSION:
            return FileType.JSON
        elif file_extension in _XML_FILE_EXTENSIONS:
            return FileType.XML
        elif file_extension == _PNG_FILE_EXTENSION:
            return FileType.PNG
        elif (file_extension in _TEXT_FILE_EXTENSIONS):
            return FileType.TEXT
        else:
            return FileType.NONE

    def _create_checker(self, file_type, file_path, handle_style_error,
                        min_confidence):
        """Instantiate and return a style checker based on file type."""
        if file_type == FileType.NONE:
            checker = None
        elif file_type == FileType.CPP:
            file_extension = self._file_extension(file_path)
            checker = CppChecker(file_path, file_extension, handle_style_error,
                                 min_confidence)
        elif file_type == FileType.JSON:
            checker = JSONChecker(file_path, handle_style_error)
        elif file_type == FileType.XML:
            checker = XMLChecker(file_path, handle_style_error)
        elif file_type == FileType.PNG:
            checker = PNGChecker(file_path, handle_style_error)
        elif file_type == FileType.TEXT:
            checker = TextChecker(file_path, handle_style_error)
        else:
            raise ValueError(
                'Invalid file type "%(file_type)s": the only valid file types '
                'are %(NONE)s, %(CPP)s, and %(TEXT)s.' % {
                    'file_type': file_type,
                    'NONE': FileType.NONE,
                    'CPP': FileType.CPP,
                    'TEXT': FileType.TEXT
                })

        return checker

    def dispatch(self, file_path, handle_style_error, min_confidence):
        """Instantiate and return a style checker based on file path."""
        file_type = self._file_type(file_path)

        checker = self._create_checker(file_type, file_path,
                                       handle_style_error, min_confidence)
        return checker


# FIXME: Remove the stderr_write attribute from this class and replace
#        its use with calls to a logging module logger.
class StyleProcessorConfiguration(object):
    """Stores configuration values for the StyleProcessor class.

    Attributes:
      min_confidence: An integer between 1 and 5 inclusive that is the
                      minimum confidence level of style errors to report.

      max_reports_per_category: The maximum number of errors to report
                                per category, per file.

      stderr_write: A function that takes a string as a parameter and
                    serves as stderr.write.
    """

    def __init__(self, filter_configuration, max_reports_per_category,
                 min_confidence, output_format, stderr_write):
        """Create a StyleProcessorConfiguration instance.

        Args:
          filter_configuration: A FilterConfiguration instance.  The default
                                is the "empty" filter configuration, which
                                means that all errors should be checked.

          max_reports_per_category: The maximum number of errors to report
                                    per category, per file.

          min_confidence: An integer between 1 and 5 inclusive that is the
                          minimum confidence level of style errors to report.
                          The default is 1, which reports all style errors.

          output_format: A string that is the output format.  The supported
                         output formats are "emacs" which emacs can parse
                         and "vs7" which Microsoft Visual Studio 7 can parse.

          stderr_write: A function that takes a string as a parameter and
                        serves as stderr.write.
        """
        self._filter_configuration = filter_configuration
        self._output_format = output_format

        self.max_reports_per_category = max_reports_per_category
        self.min_confidence = min_confidence
        self.stderr_write = stderr_write

    def is_reportable(self, category, confidence_in_error, file_path):
        """Return whether an error is reportable.

        An error is reportable if both the confidence in the error is
        at least the minimum confidence level and the current filter
        says the category should be checked for the given path.

        Args:
          category: A string that is a style category.
          confidence_in_error: An integer between 1 and 5 inclusive that is
                               the application's confidence in the error.
                               A higher number means greater confidence.
          file_path: The path of the file being checked
        """
        if confidence_in_error < self.min_confidence:
            return False

        return self._filter_configuration.should_check(category, file_path)

    def write_style_error(self, category, confidence_in_error, file_path,
                          line_number, message):
        """Write a style error to the configured stderr."""
        if self._output_format == 'vs7':
            format_string = '%s(%s):  %s  [%s] [%d]\n'
        else:
            format_string = '%s:%s:  %s  [%s] [%d]\n'

        self.stderr_write(format_string % (file_path, line_number, message,
                                           category, confidence_in_error))


class ProcessorBase(object):
    """The base class for processors of lists of lines."""

    def should_process(self, file_path):
        """Return whether the file at file_path should be processed.

        The TextFileReader class calls this method prior to reading in
        the lines of a file.  Use this method, for example, to prevent
        the style checker from reading binary files into memory.
        """
        raise NotImplementedError('Subclasses should implement.')

    def process(self, lines, file_path, **kwargs):
        """Process lines of text read from a file.

        Args:
          lines: A list of lines of text to process.
          file_path: The path from which the lines were read.
          **kwargs: This argument signifies that the process() method of
                    subclasses of ProcessorBase may support additional
                    keyword arguments.
                        For example, a style checker's check() method
                    may support a "reportable_lines" parameter that represents
                    the line numbers of the lines for which style errors
                    should be reported.
        """
        raise NotImplementedError('Subclasses should implement.')


class StyleProcessor(ProcessorBase):
    """A ProcessorBase for checking style.

    Attributes:
      error_count: An integer that is the total number of reported
                   errors for the lifetime of this instance.
    """

    def __init__(self,
                 configuration,
                 mock_dispatcher=None,
                 mock_increment_error_count=None,
                 mock_carriage_checker_class=None):
        """Create an instance.

        Args:
          configuration: A StyleProcessorConfiguration instance.
          mock_dispatcher: A mock CheckerDispatcher instance.  This
                           parameter is for unit testing.  Defaults to a
                           CheckerDispatcher instance.
          mock_increment_error_count: A mock error-count incrementer.
          mock_carriage_checker_class: A mock class for checking and
                                       transforming carriage returns.
                                       This parameter is for unit testing.
                                       Defaults to CarriageReturnChecker.
        """
        if mock_dispatcher is None:
            dispatcher = CheckerDispatcher()
        else:
            dispatcher = mock_dispatcher

        if mock_increment_error_count is None:
            def increment_error_count():
                """Increment the total count of reported errors."""
                self.error_count += 1
        else:
            increment_error_count = mock_increment_error_count

        if mock_carriage_checker_class is None:
            # This needs to be a class rather than an instance since the
            # process() method instantiates one using parameters.
            carriage_checker_class = CarriageReturnChecker
        else:
            carriage_checker_class = mock_carriage_checker_class

        self.error_count = 0

        self._carriage_checker_class = carriage_checker_class
        self._configuration = configuration
        self._dispatcher = dispatcher
        self._increment_error_count = increment_error_count

    def should_process(self, file_path):
        """Return whether the file should be checked for style."""
        if self._dispatcher.should_skip_without_warning(file_path):
            return False
        return True

    def process(self, lines, file_path, line_numbers=None):
        """Check the given lines for style.

        Arguments:
          lines: A list of all lines in the file to check.
          file_path: The path of the file to process.  If possible, the path
                     should be relative to the source root.  Otherwise,
                     path-specific logic may not behave as expected.
          line_numbers: A list of line numbers of the lines for which
                        style errors should be reported, or None if errors
                        for all lines should be reported.  When not None, this
                        list normally contains the line numbers corresponding
                        to the modified lines of a patch.
        """
        _log.debug('Checking style: ' + file_path)

        style_error_handler = DefaultStyleErrorHandler(
            configuration=self._configuration,
            file_path=file_path,
            increment_error_count=self._increment_error_count,
            line_numbers=line_numbers)

        carriage_checker = self._carriage_checker_class(style_error_handler)

        # Check for and remove trailing carriage returns ("\r").
        if self._dispatcher.should_check_and_strip_carriage_returns(file_path):
            lines = carriage_checker.check(lines)

        min_confidence = self._configuration.min_confidence
        checker = self._dispatcher.dispatch(file_path, style_error_handler,
                                            min_confidence)

        if checker is None:
            raise AssertionError(
                "File should not be checked: '%s'" % file_path)

        _log.debug('Using class: ' + checker.__class__.__name__)

        checker.check(lines)

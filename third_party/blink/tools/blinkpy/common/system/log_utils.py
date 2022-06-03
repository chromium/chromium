# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""Supports blinkpy logging."""

import logging
import sys

_log = logging.getLogger(__name__)


def _default_handlers(stream, logging_level, include_time):
    """Return a list of the default logging handlers to use.

    Args:
      stream: See the configure_logging() docstring.
      include_time: See the configure_logging() docstring.
    """

    # Create the filter.
    def should_log(record):
        """Return whether a logging.LogRecord should be logged."""
        if record.name.startswith('blinkpy.third_party'):
            return False
        return True

    logging_filter = logging.Filter()
    logging_filter.filter = should_log

    # Create the handler.
    handler = logging.StreamHandler(stream)
    if include_time:
        prefix = '%(asctime)s - '
    else:
        prefix = ''

    if logging_level == logging.DEBUG:
        formatter = logging.Formatter(prefix +
                                      '%(name)s: [%(levelname)s] %(message)s')
    else:
        formatter = logging.Formatter(prefix + '%(message)s')

    handler.setFormatter(formatter)
    handler.addFilter(logging_filter)

    return [handler]


def configure_logging(logging_level=None,
                      logger=None,
                      stream=None,
                      handlers=None,
                      include_time=True):
    """Configure logging for standard purposes.

    Returns:
      A list of references to the logging handlers added to the root
      logger.  This allows the caller to later remove the handlers
      using logger.removeHandler.  This is useful primarily during unit
      testing where the caller may want to configure logging temporarily
      and then undo the configuring.

    Args:
      logging_level: The minimum logging level to log.  Defaults to
                     logging.INFO.
      logger: A logging.logger instance to configure.  This parameter
              should be used only in unit tests.  Defaults to the
              root logger.
      stream: A file-like object to which to log used in creating the default
              handlers.  The stream must define an "encoding" data attribute,
              or else logging raises an error.  Defaults to sys.stderr.
      handlers: A list of logging.Handler instances to add to the logger
                being configured.  If this parameter is provided, then the
                stream parameter is not used.
      include_time: Include time information at the start of every log message.
                    Useful for understanding how much time has passed between
                    subsequent log messages.
    """
    # If the stream does not define an "encoding" data attribute, the
    # logging module can throw an error like the following:
    #
    # Traceback (most recent call last):
    #   File "/System/Library/Frameworks/Python.framework/Versions/2.6/...
    #         lib/python2.6/logging/__init__.py", line 761, in emit
    #     self.stream.write(fs % msg.encode(self.stream.encoding))
    # LookupError: unknown encoding: unknown
    if logging_level is None:
        logging_level = logging.INFO
    if logger is None:
        logger = logging.getLogger()
    if stream is None:
        stream = sys.stderr
    if handlers is None:
        handlers = _default_handlers(stream, logging_level, include_time)

    logger.setLevel(logging_level)

    for handler in handlers:
        logger.addHandler(handler)

    _log.debug('Debug logging enabled.')

    return handlers

#!/usr/bin/env vpython3
# Copyright (c) 2011 Code Aurora Forum. All rights reserved.
# Copyright (c) 2010 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
# Copyright (C) 2010 Chris Jerdonek (cjerdonek@webkit.org)
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

import codecs
import logging
import os
import signal
import sys
import six

from blinkpy.common.system.log_utils import configure_logging
from blinkpy.tool.blink_tool import BlinkTool

# A StreamWriter will by default try to encode all objects passed
# to write(), so when passed a raw string already encoded as utf8,
# it will blow up with an UnicodeDecodeError. This does not match
# the default behaviour of writing to sys.stdout, so we intercept
# the case of writing raw strings and make sure StreamWriter gets
# input that it can handle.


class ForgivingUTF8Writer(codecs.lookup('utf-8')[-1]):
    def write(self, obj):
        if isinstance(obj, str):
            # Assume raw strings are utf-8 encoded. If this line
            # fails with an UnicodeDecodeError, our assumption was
            # wrong, and the stacktrace should show you where we
            # write non-Unicode/UTF-8 data (which we shouldn't).
            obj = obj.decode('utf-8')
        return codecs.StreamWriter.write(self, obj)


# By default, sys.stdout assumes ascii encoding.  Since our messages can
# contain unicode strings (as with some peoples' names) we need to apply
# the utf-8 codec to prevent throwing and exception.
# Not having this was the cause of https://bugs.webkit.org/show_bug.cgi?id=63452.
# In PY3 default encoding is utf-8. Hence we don't need this.
if six.PY2:
    sys.stdout = ForgivingUTF8Writer(sys.stdout)


def main() -> int:
    # This is a hack to let us enable DEBUG logging as early as possible.
    # Note this can't be ternary as versioning.check_version()
    # hasn't run yet and this python might be older than 2.5.
    if set(["-v", "--verbose"]).intersection(set(sys.argv)):
        logging_level = logging.DEBUG
    else:
        logging_level = logging.INFO
    configure_logging(logging_level=logging_level)
    return BlinkTool(os.path.abspath(__file__)).main()


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(signal.SIGINT + 128)

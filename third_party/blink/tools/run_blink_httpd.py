#!/usr/bin/env vpython3
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
"""Runs an Apache HTTP server to manually run web tests locally.

After running this script, you can locally navigate to URLs where
the path is relative to web_tests/http/tests/. For example, to run
web_tests/http/tests/cachestorage/window-cache-add.html, navigate to:
    http://127.0.0.1:8000/cachestorage/window/cache-add.html

When using HTTPS, for example:
    https://127.0.0.1:8443/https/verify-ssl-enabled.php
you will may a certificate warning, which you need to bypass.

After starting the server, you can also run individual web tests
via content_shell, e.g.
    $ out/Release/content_shell --run-web-tests \
    http://127.0.0.1:8000/security/cross-frame-access-get.html

Note that some tests will only work if "127.0.0.1" for the host part of the
URL, rather than "localhost".
"""

from blinkpy.web_tests.servers import cli_wrapper
from blinkpy.web_tests.servers import apache_http

cli_wrapper.main(
    apache_http.ApacheHTTP,
    additional_dirs={},
    number_of_servers=4,
    description=__doc__)

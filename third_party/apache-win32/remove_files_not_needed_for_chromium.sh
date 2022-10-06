#!/bin/bash -e

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a simple script that strips out all of the files from the
# Apache distribution we don't actually need to run the Chromium
# layout tests.

find . \! \( \
    -path . -o \
    -path .. -o \
    -path ./ABOUT_APACHE.txt -o \
    -path ./bin -o \
    -path ./bin/ApacheMonitor.exe -o \
    -path ./bin/httpd.exe -o \
    -path ./bin/libapr-1.dll -o \
    -path ./bin/libapriconv-1.dll -o \
    -path ./bin/libaprutil-1.dll -o \
    -path ./bin/libcrypto-1_1-x64.dll -o \
    -path ./bin/libhttpd.dll -o \
    -path ./bin/libssl-1_1-x64.dll -o \
    -path ./bin/openssl.exe -o \
    -path ./bin/pcre.dll -o \
    -path ./bin/php7ts.dll -o \
    -path ./bin/zlib1.dll -o \
    -path ./LICENSE.txt -o \
    -path ./modules -o \
    -path ./modules/mod_access_compat.so -o \
    -path ./modules/mod_alias.so -o \
    -path ./modules/mod_asis.so -o \
    -path ./modules/mod_authn_core.so -o \
    -path ./modules/mod_authz_core.so -o \
    -path ./modules/mod_authz_host.so -o \
    -path ./modules/mod_autoindex.so -o \
    -path ./modules/mod_cgi.so -o \
    -path ./modules/mod_env.so -o \
    -path ./modules/mod_headers.so -o \
    -path ./modules/mod_log_config.so -o \
    -path ./modules/mod_mime.so -o \
    -path ./modules/mod_rewrite.so -o \
    -path ./modules/mod_ssl.so -o \
    -path ./modules/php7apache2_4.dll -o \
    -path ./NOTICE.txt -o \
    -path ./OPENSSL-README.txt -o \
    -path ./OWNERS -o \
    -path ./README-win32.txt -o \
    -path ./README.chromium -o \
    -path ./README.txt -o \
    -path ./readme-redist-bins.txt -o \
    -path ./.gitignore -o \
    -path ./remove_files_not_needed_for_chromium.sh \
    \) | xargs rm -fr

#!/usr/bin/python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper to update the "net_unittests_bundle_data" section of
//net/BUILD.gn so that it lists all of the current files (based on
simpler globbing rules).
"""

import glob
import os
import re
import sys


# ------------------------------------------
# test_support_bundle_data
# ------------------------------------------

# This is a bit more expansive than it needs to be (includes README). Meh.
test_support_bundle_data_globs = [
    "data/quic_http_response_cache_data_with_push/test.example.com/*",
    "data/ssl/certificates/*",
]

# This regular expression identifies the "sources" section of
# test_support_bundle_data.
test_support_bundle_data_regex = re.compile(r"""
bundle_data\("test_support_bundle_data"\) \{
  visibility = \[ ":test_support" \]
  testonly = true
  sources = \[
(.+?)
  \]
  outputs = \[""", re.MULTILINE | re.DOTALL)

# ------------------------------------------
# net_unittest_bundle_data
# ------------------------------------------

net_unittest_bundle_data_globs = [
    "data/cert_issuer_source_aia_unittest/*.pem",
    "data/cert_issuer_source_static_unittest/*.pem",
    "data/certificate_policies_unittest/*.pem",
    "data/crl_unittest/*.pem",
    "data/embedded_test_server/*",
    "data/filter_unittests/*",
    "data/name_constraints_unittest/*.pem",
    "data/ocsp_unittest/*.pem",
    "data/ov_name_constraints/*.pem",
    "data/path_builder_unittest/**/*.pem",
    "data/parse_certificate_unittest/**/*.pem",
    "data/parse_certificate_unittest/*.pem",
    "data/parse_certificate_unittest/*.pk8",
    "data/test.html",
    "data/trial_comparison_cert_verifier_unittest/**/*.pem",
    "data/url_request_unittest/*",
    "data/verify_certificate_chain_unittest/**/*.pem",
    "data/verify_certificate_chain_unittest/**/*.test",
    "data/verify_certificate_chain_unittest/pkits_errors/*.txt",
    "data/verify_name_match_unittest/names/*.pem",
    "data/verify_signed_data_unittest/*.pem",
    "third_party/nist-pkits/certs/*.crt",
    "third_party/nist-pkits/crls/*.crl",
]

# This regular expression identifies the "sources" section of
# net_unittests_bundle_data
net_unittest_bundle_data_regex = re.compile(r"""
bundle_data\("net_unittests_bundle_data"\) \{
  testonly = true
  sources = \[
(.+?)
  \]
  outputs = \[""", re.MULTILINE | re.DOTALL)

# ------------------------------------------

def get_net_path():
  """Returns the path to //net"""
  return os.path.realpath(os.path.join(os.path.dirname(__file__), os.pardir))


def do_file_glob(rule):
  # Do the globbing relative to //net
  prefix = get_net_path()
  matches = glob.glob(prefix + os.sep + rule)

  # Strip off the prefix.
  return [f[len(prefix) + 1:] for f in matches]


def resolve_file_globs(rules):
  paths = []
  for rule in rules:
    paths.extend(do_file_glob(rule))
  return paths


def read_file_to_string(path):
  with open(path, 'r') as f:
    return f.read()


def write_string_to_file(data, path):
  with open(path, 'w') as f:
    f.write(data)


def fatal(message):
  print "FATAL: " + message
  sys.exit(1)


def format_file_list(files):
  # Keep the file list in sorted order.
  files = sorted(files)

  # Format to a string for GN (assume the filepaths don't contain
  # characters that need escaping).
  return ",\n".join('    "%s"' % f for f in files) + ","


def replace_sources(data, sources_regex, globs):
  m = sources_regex.search(data)
  if not m:
      fatal("Couldn't find the sources section: %s" % sources_regex.pattern)

  formatted_files = format_file_list(resolve_file_globs(globs))
  return data[0:m.start(1)] + formatted_files + data[m.end(1):]


def main():
  # Read in //net/BUILD.gn
  path = os.path.join(get_net_path(), "BUILD.gn")
  data = read_file_to_string(path)

  # Replace the sources part of "net_unittests_bundle_data" with
  # the current results of file globbing.
  data = replace_sources(data, test_support_bundle_data_regex,
                         test_support_bundle_data_globs)

  # Replace the sources part of "net_unittests_bundle_data" with
  # the current results of file globbing.
  data = replace_sources(data, net_unittest_bundle_data_regex,
                         net_unittest_bundle_data_globs)

  write_string_to_file(data, path)
  print "Wrote %s" % path


if __name__ == '__main__':
  sys.exit(main())

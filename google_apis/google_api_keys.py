#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Python API for retrieving API keys.

Note that this does not have the exact same semantics as the C++ API
in google_api_keys.h, since it does not have access to gyp variables
or preprocessor defines.
"""

from __future__ import print_function

import os
import re
import sys


# The token returned when an API key is unset.
DUMMY_TOKEN = 'dummytoken'


def _GetTokenFromOfficialFile(token_name):
  """Parses the token from the official file if it exists, else returns None."""
  official_path = os.path.join(os.path.dirname(__file__),
                               'internal/google_chrome_api_keys.h')
  if not os.path.isfile(official_path):
    return None

  line_regexp = r'^#define\s*%s\s*"([^"]+)"' % token_name
  line_pattern = re.compile(line_regexp)
  def ParseLine(current_line):
    result = line_pattern.match(current_line)
    if result:
      return result.group(1)
    else:
      return None

  with open(official_path) as f:
    current_line = ''
    for line in f:
      if line.endswith('\\\n'):
        current_line += line[:-2]
      else:
        # Last line in multi-line #define, or a line that is not a
        # continuation line.
        current_line += line
        token = ParseLine(current_line)
        if token:
          if current_line.count('"') != 2:
            raise Exception(
              'Embedded quotes and multi-line strings are not supported.')
          return token
        current_line = ''
    return None


def _GetToken(token_name):
  """Returns the API token with the given name, or DUMMY_TOKEN by default."""
  if token_name in os.environ:
    return os.environ[token_name]
  token = _GetTokenFromOfficialFile(token_name)
  if token:
    return token
  else:
    return DUMMY_TOKEN


def GetAPIKey():
  """Returns the simple API key."""
  return _GetToken('GOOGLE_API_KEY')


def GetAPIKeyAndroidNonStable():
  """Returns the API key for android non-stable channel."""
  return _GetToken('GOOGLE_API_KEY_ANDROID_NON_STABLE')


def GetClientID(client_name):
  """Returns the OAuth 2.0 client ID for the client of the given name."""
  return _GetToken('GOOGLE_CLIENT_ID_%s' % client_name)


def GetClientSecret(client_name):
  """Returns the OAuth 2.0 client secret for the client of the given name."""
  return _GetToken('GOOGLE_CLIENT_SECRET_%s' % client_name)


if __name__ == "__main__":
  print('GOOGLE_API_KEY=%s' % GetAPIKey())
  print('GOOGLE_CLIENT_ID_MAIN=%s' % GetClientID('MAIN'))
  print('GOOGLE_CLIENT_SECRET_MAIN=%s' % GetClientSecret('MAIN'))
  print('GOOGLE_CLIENT_ID_REMOTING=%s' % GetClientID('REMOTING'))
  print('GOOGLE_CLIENT_SECRET_REMOTING=%s' % GetClientSecret('REMOTING'))
  print('GOOGLE_CLIENT_ID_REMOTING_HOST=%s' % GetClientID('REMOTING_HOST'))
  print('GOOGLE_CLIENT_SECRET_REMOTING_HOST=%s' % GetClientSecret(
      'REMOTING_HOST'))

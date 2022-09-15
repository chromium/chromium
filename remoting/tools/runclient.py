#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Gets the chromoting host info from an input arg and then
tries to find the authentication info in the .chromotingAuthToken file
so that the host authentication arguments can be automatically set.
"""

import os
import platform
import sys

def main():
  auth_filepath = os.path.join(os.path.expanduser('~'), '.chromotingAuthToken')
  script_path = os.path.dirname(__file__)

  if platform.system() == "Windows":
    # TODO(garykac): Make this work on Windows.
    print 'Not yet supported on Windows.'
    return 1
  elif platform.system() == "Darwin": # Darwin == MacOSX
    client_path = '../../xcodebuild/Debug/chromoting_simple_client'
  else:
    client_path = '../../out/Debug/chromoting_x11_client'

  client_path = os.path.join(script_path, client_path)

  # Read username and auth token from token file.
  auth = open(auth_filepath)
  authinfo = auth.readlines()

  username = authinfo[0].rstrip()
  authtoken = authinfo[1].rstrip()

  # Request final 8 characters of Host JID from user.
  # This assumes that the host is published under the same username as the
  # client attempting to connect.
  print 'Host JID:', username + '/chromoting',
  hostjid_suffix = raw_input()
  hostjid = username + '/chromoting' + hostjid_suffix.upper()

  command = []
  command.append(client_path)
  command.append('--host_jid ' + hostjid)
  command.append('--jid ' + username)
  command.append('--token ' + authtoken)

  # Launch the client
  os.system(' '.join(command))
  return 0


if __name__ == '__main__':
  sys.exit(main())

# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Each CLSID is a hash of the current version string salted with an
# arbitrary GUID. This ensures that the newly installed COM classes will
# be used during/after upgrade even if there are old instances running
# already.
# The IDs are not random to avoid rebuilding host when it's not
# necessary.

from __future__ import print_function

import uuid
import sys

if len(sys.argv) != 3:
  print("""Expecting 2 args:
<rdp_desktop_session_guid> <version>""")
  sys.exit(1)

rdp_desktop_session_guid = sys.argv[1]
version_full = sys.argv[2]

# Output a GN list of 1 strings.
print('["' + \
    str(uuid.uuid5(uuid.UUID(rdp_desktop_session_guid), version_full)) + '"]')


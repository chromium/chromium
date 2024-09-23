#!/usr/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.common import exit_codes
from blinkpy.common.host import Host
from blinkpy.web_tests.vts_notifier import VTSNotifier


def main():
    host = Host()
    notifier = VTSNotifier(host)
    try:
        notifier.run()
    except KeyboardInterrupt:
        host.print_('Interrupted, exiting')
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()

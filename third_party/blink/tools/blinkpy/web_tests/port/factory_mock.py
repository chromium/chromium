# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.port.test import TestPort


class MockPortFactory(object):
    def __init__(self, host):
        self._host = host

    def get(self, port_name=None):
        return TestPort(port_name=port_name, host=self._host)

    def get_from_builder_name(self, builder_name):
        port_name = self._host.builders.port_name_for_builder_name(
            builder_name)
        return self.get(port_name)

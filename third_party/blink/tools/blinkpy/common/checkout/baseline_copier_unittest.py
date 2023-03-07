# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
from unittest import mock

from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.common.checkout.baseline_optimizer_unittest import BaselineTest
from blinkpy.web_tests.port.test import TestPort


class BaselineCopierTest(BaselineTest):
    def setUp(self):
        super().setUp()
        port_names = frozenset().union(*TestPort.FALLBACK_PATHS.values())
        self._mocks = contextlib.ExitStack()
        self._mocks.enter_context(
            mock.patch.object(self.host.port_factory,
                              'all_port_names',
                              return_value=port_names))
        default_port = self._port('linux-trusty')
        self.copier = BaselineCopier(self.host, default_port)

    def tearDown(self):
        super().tearDown()
        self._mocks.close()

    def _port(self, name):
        return self.host.port_factory.get(port_name=('test-%s' % name))

    def test_demote(self):
        """Verify that generic baselines are copied down.

        Demotion is the inverse operation of the promotion step in the
        optimizer. This ensures rebaselining leaves behind a generic baseline
        if and only if it was promoted. That is, we prefer Figure 1 over 2,
        where (*) denotes a file that exists. Both states are equivalent, but
        Figure 2's generic baseline can be misleading.

              (generic)               (generic)*
             /         \             /         \
             |         |             |         |
            win*      mac*          win*      mac

              Figure 1                Figure 2

        """
        self._write_baselines(
            'failures/expected/image-expected.txt', {
                '': 'generic result',
                'virtual/virtual_failures/': 'generic virtual result',
            })
        self._assert_baselines(
            'failures/expected/image-expected.txt', {
                'platform/test-win-win10': None,
                'platform/test-mac-mac10.11': None,
                'platform/test-win-win10/virtual/virtual_failures/': None,
                'platform/test-mac-mac10.11/virtual/virtual_failures/': None,
            })
        self.copier.write_copies(
            self.copier.find_baselines_to_copy('failures/expected/image.html',
                                               'txt',
                                               [self._port('win-win10')]))
        self._assert_baselines(
            'failures/expected/image-expected.txt',
            {
                'platform/test-win-win10':
                None,  # Will be rebaselined
                'platform/test-win-win7':
                'generic result',
                'platform/test-mac-mac10.11':
                'generic result',
                'platform/test-win-win10/virtual/virtual_failures/':
                'generic virtual result',
                'platform/test-mac-mac10.11/virtual/virtual_failures/':
                'generic virtual result',
            })

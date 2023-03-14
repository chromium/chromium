# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from blinkpy.common.checkout.baseline_copier import BaselineCopier
from blinkpy.tool.commands.rebaseline import AbstractRebaseliningCommand

_log = logging.getLogger(__name__)


class CopyExistingBaselines(AbstractRebaseliningCommand):
    name = 'copy-existing-baselines-internal'
    help_text = ('Copy existing baselines down one level in the baseline '
                 'order to ensure new baselines don\'t break existing passing '
                 'platforms.')

    def __init__(self, tool):
        super().__init__(options=[
            self.test_option,
            self.suffixes_option,
            self.port_name_option,
            self.flag_specific_option,
            self.results_directory_option,
        ])
        self._copier = BaselineCopier(tool)

    def execute(self, options, args, tool):
        # TODO(crbug.com/1324638): Remove this command. Have `rebaseline-cl`
        # call `find_baselines_to_copy(...)` directly to find implied all-pass
        # baselines, then copy all ports/tests together if OK.
        port = tool.port_factory.get(options.port_name)
        if options.flag_specific:
            port.set_option_default('flag_specific', options.flag_specific)
        for suffix in options.suffixes.split(','):
            self._copier.write_copies(
                self._copier.find_baselines_to_copy(options.test, suffix,
                                                    [port]))

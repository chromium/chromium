#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gperf
import json5_generator
import template_expander


class AtRuleNamesWriter(json5_generator.Writer):
    """
    Generates AtRuleNames. This class provides utility methods for parsing
    @rules (e.g. @font-face, @keyframes, etc)
    """

    def __init__(self, json5_file_paths, output_dir):
        super(AtRuleNamesWriter, self).__init__(json5_file_paths, output_dir)

        self._outputs = {
            'at_rule_descriptors.h': self.generate_header,
            'at_rule_descriptors.cc': self.generate_implementation
        }

        self._descriptors = self.json5_file.name_dictionaries
        self._character_offsets = []

        # AtRuleDescriptorID::Invalid is 0.
        first_descriptor_id = 1
        # Aliases are resolved immediately at parse time, and thus don't appear
        # in the enum.
        self._descriptors_count = len(self._descriptors) + first_descriptor_id
        chars_used = 0
        self._longest_name_length = 0
        for offset, descriptor in enumerate(self._descriptors):
            descriptor['enum_value'] = first_descriptor_id + offset
            self._character_offsets.append(chars_used)
            chars_used += len(descriptor['name'].original)
            self._longest_name_length = max(
                len(descriptor['name'].original), len(descriptor['alias']),
                self._longest_name_length)

    @template_expander.use_jinja(
        'core/css/parser/templates/at_rule_descriptors.h.tmpl')
    def generate_header(self):
        return {
            'descriptors': self._descriptors,
            'descriptors_count': self._descriptors_count
        }

    @gperf.use_jinja_gperf_template(
        'core/css/parser/templates/at_rule_descriptors.cc.tmpl')
    def generate_implementation(self):
        return {
            'descriptors': self._descriptors,
            'descriptor_offsets': self._character_offsets,
            'descriptors_count': len(self._descriptors),
            'longest_name_length': self._longest_name_length,
            'gperf_path': self.gperf_path
        }


if __name__ == '__main__':
    json5_generator.Maker(AtRuleNamesWriter).main()

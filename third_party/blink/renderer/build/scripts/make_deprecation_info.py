#!/usr/bin/env python
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import template_expander


class DeprecationInfoWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(DeprecationInfoWriter, self).__init__(json5_file_paths,
                                                    output_dir)
        self._outputs = {
            'deprecation_info.cc': self.generate_implementation,
        }

    @template_expander.use_jinja('templates/deprecation_info.cc.tmpl')
    def generate_implementation(self):
        return {
            'input_files': self._input_files,
            'deprecations': self.json5_file.name_dictionaries,
        }


if __name__ == '__main__':
    json5_generator.Maker(DeprecationInfoWriter).main()

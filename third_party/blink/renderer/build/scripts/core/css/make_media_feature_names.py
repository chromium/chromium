#!/usr/bin/env python

# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_names
import media_feature_symbol


class MakeMediaFeatureNamesWriter(make_names.MakeNamesWriter):
    def __init__(self, json5_file_path, output_dir):
        super(MakeMediaFeatureNamesWriter, self).__init__(
            json5_file_path, output_dir)
        MakeMediaFeatureNamesWriter.filters['symbol'] = (
            media_feature_symbol.getMediaFeatureSymbolWithSuffix(
                'MediaFeature'))


if __name__ == "__main__":
    json5_generator.Maker(MakeMediaFeatureNamesWriter).main()

# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import sys

PYJSON5_DIR = os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..', 'pyjson5', 'src')
sys.path.insert(0, PYJSON5_DIR)

import json5  # pylint: disable=import-error


class ARIAReader(object):
    def __init__(self, json5_file_path):
        self._input_files = [json5_file_path]

        with open(os.path.abspath(json5_file_path)) as json5_file:
            self._data = json5.loads(json5_file.read())

    def attributes_list(self):
        return {'data': [item[u'name'] for item in self._data['attributes']]}

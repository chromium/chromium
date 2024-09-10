# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from unittest import mock

from bad_machine_finder import swarming


class GetBotIdFromTaskUnittest(unittest.TestCase):

  def testBasic(self):
    """Tests behavior along the basic/happy path."""
    swarming_output = {
        'id': {
            'results': {
                'bot_dimensions': [
                    {
                        'key': 'os',
                        'value': [
                            'Mac',
                            'Mac-14',
                        ],
                    },
                    {
                        'key': 'id',
                        'value': [
                            'mac-123-e505',
                        ],
                    },
                ],
            },
        },
    }
    with mock.patch.object(swarming.subprocess,
                           'check_output',
                           return_value=json.dumps(swarming_output)):
      bot_id = swarming.GetBotIdFromTask('id')
    self.assertEqual(bot_id, 'mac-123-e505')

  def testNoId(self):
    """Tests behavior when no ID is present."""
    swarming_output = {
        'id': {
            'results': {
                'bot_dimensions': [
                    {
                        'key': 'os',
                        'value': [
                            'Mac',
                            'Mac-14',
                        ],
                    },
                ],
            },
        },
    }
    with mock.patch.object(swarming.subprocess,
                           'check_output',
                           return_value=json.dumps(swarming_output)):
      with self.assertRaisesRegex(RuntimeError,
                                  'Could not find bot ID for task id'):
        swarming.GetBotIdFromTask('id')

#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_buildbot_json.py."""

import argparse
import os
import unittest

import generate_buildbot_json


class FakeBBGen(generate_buildbot_json.BBJSONGenerator):
  def __init__(self, waterfalls, test_suites, exceptions, mixins,
               luci_milo_cfg):
    super(FakeBBGen, self).__init__()
    infra_config_dir = os.path.abspath(
        os.path.join(os.path.dirname(__file__), '..', '..',
                    'infra', 'config', 'global'))
    luci_milo_cfg_path = os.path.join(infra_config_dir, 'luci-milo.cfg')
    luci_milo_dev_cfg_path = os.path.join(infra_config_dir, 'luci-milo-dev.cfg')
    self.files = {
      'waterfalls.pyl': waterfalls,
      'test_suites.pyl': test_suites,
      'test_suite_exceptions.pyl': exceptions,
      'mixins.pyl': mixins,
      luci_milo_cfg_path: luci_milo_cfg,
      luci_milo_dev_cfg_path: '',
    }
    self.printed_lines = []

  def print_line(self, line):
    self.printed_lines.append(line)

  def read_file(self, relative_path):
    return self.files[relative_path]

  def write_file(self, relative_path, contents):
    self.files[relative_path] = contents

  # pragma pylint: disable=arguments-differ
  def check_output_file_consistency(self, verbose=False, dump=True):
    try:
      super(FakeBBGen, self).check_output_file_consistency(verbose)
    except generate_buildbot_json.BBGenErr:
      if verbose and dump:
          # Assume we want to see the difference in the waterfalls'
          # generated output to make it easier to rebaseline the test.
          for line in self.printed_lines:
            print line
      raise
# pragma pylint: enable=arguments-differ


FOO_GTESTS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {
          'dimension_sets': [
            {
              'kvm': '1',
            },
          ],
        },
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_WITH_ENABLE_FEATURES_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
        'args': [
          '--enable-features=Baz',
        ],
      },
    },
  },
]
"""

FOO_GTESTS_MULTI_DIMENSION_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {
          'dimension_sets': [
            {
              "gpu": "none",
              "os": "1",
            },
          ],
        },
        'use_multi_dimension_trigger_script': True,
        'alternate_swarming_dimensions': [
          {
            "gpu": "none",
            "os": "2",
          },
        ],
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""


FOO_LINUX_GTESTS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'linux',
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

COMPOSITION_GTEST_SUITE_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

COMPOSITION_GTEST_SUITE_WITH_ARGS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'composition_tests',
        },
        'args': [
          '--this-is-an-argument',
        ],
      },
    },
  },
]
"""

FOO_ISOLATED_SCRIPTS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'isolated_scripts': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_SCRIPT_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'scripts': 'foo_scripts',
        },
      },
    },
  },
]
"""

FOO_JUNIT_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'junit_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_CTS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'cts_tests': 'foo_cts_tests',
        },
      },
    },
  },
]
"""

FOO_INSTRUMENTATION_TEST_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_GPU_TELEMETRY_TEST_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'win',
        'browser_config': 'release',
        'swarming': {
          'dimension_sets': [
            {
              'gpu': '10de:1cb3',
            },
          ],
        },
        'test_suites': {
          'gpu_telemetry_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

UNKNOWN_TEST_SUITE_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'baz_tests',
        },
      },
    },
  },
]
"""

UNKNOWN_TEST_SUITE_TYPE_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'foo_tests',
          'foo_test_type': 'foo_tests',
        },
      },
    },
  },
]
"""

ANDROID_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Android Builder': {
        'additional_compile_targets': [
          'bar_test',
        ],
      },
      'Fake Android K Tester': {
        'additional_compile_targets': [
          'bar_test',
        ],
        'swarming': {
          'dimension_sets': [
            {
              'device_os': 'KTU84P',
              'device_type': 'hammerhead',
            },
          ],
        },
        'os_type': 'android',
        'skip_merge_script': True,
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
      'Fake Android L Tester': {
        'swarming': {
          'dimension_sets': [
            {
              'device_os': 'KTU84P',
              'device_type': 'hammerhead',
            },
          ],
        },
        'os_type': 'android',
        'skip_merge_script': True,
        'skip_output_links': True,
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
      'Fake Android M Tester': {
        'swarming': {
          'dimension_sets': [
            {
              'device_os': 'MMB29Q',
              'device_type': 'bullhead',
            },
          ],
        },
        'os_type': 'android',
        'use_swarming': False,
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

UNKNOWN_BOT_GTESTS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Unknown Bot': {
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_TEST_SUITE = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'dimension_sets': [
            {
              'integrity': 'high',
            }
          ],
          'expiration': 120,
        },
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'args': [
          '--c_arg',
        ],
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_LINUX_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'linux_args': [
          '--no-xvfb',
        ],
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_ENABLE_FEATURES = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'args': [
          '--enable-features=Foo,Bar',
        ],
      },
    },
  },
}
"""


FOO_SCRIPT_SUITE = """\
{
  'basic_suites': {
    'foo_scripts': {
      'foo_test': {
        'script': 'foo.py',
      },
      'bar_test': {
        'script': 'bar.py',
      },
    },
  },
}
"""

FOO_CTS_SUITE = """\
{
  'basic_suites': {
    'foo_cts_tests': {
      'arch': 'arm64',
      'platform': 'L',
    },
  },
}
"""

GOOD_COMPOSITION_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {
      'bar_test': {},
    },
    'foo_tests': {
      'foo_test': {},
    },
  },
  'compound_suites': {
    'composition_tests': [
      'foo_tests',
      'bar_tests',
    ],
  },
}
"""

BAD_COMPOSITION_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {},
    'foo_tests': {},
  },
  'compound_suites': {
    'buggy_composition_tests': [
      'bar_tests',
    ],
    'composition_tests': [
      'foo_tests',
      'buggy_composition_tests',
    ],
  },
}
"""

DUPLICATES_COMPOSITION_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {},
    'foo_tests': {},
  },
  'compound_suites': {
    'bar_tests': [
      'foo_tests',
    ],
    'composition_tests': [
      'foo_tests',
      'buggy_composition_tests',
    ],
  },
}
"""

INSTRUMENTATION_TESTS_WITH_DIFFERENT_NAMES = """\
{
  'basic_suites': {
    'composition_tests': {
      'foo_tests': {
        'test': 'foo_test',
      },
      'bar_tests': {
        'test': 'foo_test',
      },
    },
  },
}
"""

SCRIPT_SUITE = """\
{
  'basic_suites': {
    'foo_scripts': {
      'foo_test': {
        'script': 'foo.py',
      },
    },
  },
}
"""

UNREFED_TEST_SUITE = """\
{
  'basic_suites': {
    'bar_tests': {},
    'foo_tests': {},
  },
}
"""

REUSING_TEST_WITH_DIFFERENT_NAME = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
      'variation_test': {
        'args': [
          '--variation',
        ],
        'test': 'foo_test',
      },
    },
  },
}
"""

COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo': {},
    },
    'bar_tests': {
      'bar_test': {},
    },
  },
  'compound_suites': {
    'composition_tests': [
      'foo_tests',
      'bar_tests',
    ],
  },
}
"""

EMPTY_PYL_FILE = """\
{
}
"""

SCRIPT_WITH_ARGS_EXCEPTIONS = """\
{
  'foo_test': {
    'modifications': {
      'Fake Tester': {
        'args': ['--fake-arg'],
      },
    },
  },
}
"""

SCRIPT_WITH_ARGS_SWARMING_EXCEPTIONS = """\
{
  'foo_test': {
    'modifications': {
      'Fake Tester': {
        'swarming': {
          'value': 'exception',
        },
      },
    },
  },
}
"""

NO_BAR_TEST_EXCEPTIONS = """\
{
  'bar_test': {
    'remove_from': [
      'Fake Tester',
    ]
  }
}
"""

EMPTY_BAR_TEST_EXCEPTIONS = """\
{
  'bar_test': {
  }
}
"""

EXCEPTIONS_SORTED = """\
{
  'suite_c': {
    'modifications': {
      'Fake Tester': {
        'foo': 'bar',
      },
    },
  },
  'suite_d': {
    'modifications': {
      'Fake Tester': {
        'foo': 'baz',
      },
    },
  },
}
"""

EXCEPTIONS_UNSORTED = """\
{
  'suite_d': {
    'modifications': {
      'Fake Tester': {
        'foo': 'baz',
      },
    },
  },
  'suite_c': {
    'modifications': {
      'Fake Tester': {
        'foo': 'bar',
      },
    },
  },
}
"""

FOO_TEST_MODIFICATIONS = """\
{
  'foo_test': {
    'modifications': {
      'Fake Tester': {
        'args': [
          '--bar',
        ],
        'swarming': {
          'hard_timeout': 600,
        },
      },
    },
  }
}
"""

NONEXISTENT_REMOVAL = """\
{
  'foo_test': {
    'remove_from': [
      'Nonexistent Tester',
    ]
  }
}
"""

NONEXISTENT_MODIFICATION = """\
{
  'foo_test': {
    'modifications': {
      'Nonexistent Tester': {
        'args': [],
      },
    },
  }
}
"""

COMPOSITION_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "bar_test"
      },
      {
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

COMPOSITION_WATERFALL_WITH_ARGS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--this-is-an-argument"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "bar_test"
      },
      {
        "args": [
          "--this-is-an-argument"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

VARIATION_GTEST_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ]
        },
        "test": "foo_test"
      },
      {
        "args": [
          "--variation"
        ],
        "name": "variation_test",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ]
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

COMPOSITION_WATERFALL_FILTERED_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MERGED_ARGS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--c_arg",
          "--bar"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ],
          "hard_timeout": 600
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

LINUX_ARGS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--no-xvfb"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MERGED_ENABLE_FEATURES_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--enable-features=Foo,Bar,Baz"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MODIFIED_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--bar"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high",
              "kvm": "1"
            }
          ],
          "expiration": 120,
          "hard_timeout": 600
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

ISOLATED_SCRIPT_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "isolate_name": "foo_test",
        "name": "foo_test",
        "swarming": {
          "can_use_on_swarming_builders": true
        }
      }
    ]
  }
}
"""

SCRIPT_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "scripts": [
      {
        "name": "foo_test",
        "script": "foo.py"
      }
    ]
  }
}
"""

SCRIPT_WITH_ARGS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "scripts": [
      {
        "args": [
          "--fake-arg"
        ],
        "name": "foo_test",
        "script": "foo.py"
      }
    ]
  }
}
"""

JUNIT_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "junit_tests": [
      {
        "test": "foo_test"
      }
    ]
  }
}
"""

CTS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "cts_tests": [
      {
        "arch": "arm64",
        "platform": "L"
      }
    ]
  }
}
"""

INSTRUMENTATION_TEST_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "instrumentation_tests": [
      {
        "test": "foo_test"
      }
    ]
  }
}
"""

INSTRUMENTATION_TEST_DIFFERENT_NAMES_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "instrumentation_tests": [
      {
        "name": "bar_tests",
        "test": "foo_test"
      },
      {
        "name": "foo_tests",
        "test": "foo_test"
      }
    ]
  }
}
"""

GPU_TELEMETRY_TEST_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "foo",
          "--show-stdout",
          "--browser=release",
          "--passthrough",
          "-v",
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc"
        ],
        "isolate_name": "telemetry_gpu_integration_test",
        "name": "foo_tests",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "10de:1cb3"
            }
          ],
          "idempotent": false
        }
      }
    ]
  }
}
"""

ANDROID_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Android Builder": {
    "additional_compile_targets": [
      "bar_test"
    ]
  },
  "Fake Android K Tester": {
    "additional_compile_targets": [
      "bar_test"
    ],
    "gtest_tests": [
      {
        "args": [
          "--gs-results-bucket=chromium-result-details",
          "--recover-devices"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true,
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision": \
"git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets": [
            {
              "device_os": "KTU84P",
              "device_type": "hammerhead",
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "output_links": [
            {
              "link": [
                "https://luci-logdog.appspot.com/v/?s",
                "=android%2Fswarming%2Flogcats%2F",
                "${TASK_ID}%2F%2B%2Funified_logcats"
              ],
              "name": "shard #${SHARD_INDEX} logcats"
            }
          ]
        },
        "test": "foo_test"
      }
    ]
  },
  "Fake Android L Tester": {
    "gtest_tests": [
      {
        "args": [
          "--gs-results-bucket=chromium-result-details",
          "--recover-devices"
        ],
        "swarming": {
          "can_use_on_swarming_builders": true,
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision": \
"git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets": [
            {
              "device_os": "KTU84P",
              "device_type": "hammerhead",
              "integrity": "high"
            }
          ],
          "expiration": 120
        },
        "test": "foo_test"
      }
    ]
  },
  "Fake Android M Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": false
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MULTI_DIMENSION_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "none",
              "integrity": "high",
              "os": "1"
            }
          ],
          "expiration": 120
        },
        "test": "foo_test",
        "trigger_script": {
          "args": [
            "--multiple-trigger-configs",
            "[{\\"gpu\\": \\"none\\", \\"integrity\\": \\"high\\", \
\\"os\\": \\"1\\"}, \
{\\"gpu\\": \\"none\\", \\"os\\": \\"2\\"}]",
            "--multiple-dimension-script-verbose",
            "True"
          ],
          "script": "//testing/trigger_scripts/trigger_multiple_dimensions.py"
        }
      }
    ]
  }
}
"""

GPU_DIMENSIONS_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "foo_test",
          "--show-stdout",
          "--browser=release",
          "--passthrough",
          "-v",
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc"
        ],
        "isolate_name": "telemetry_gpu_integration_test",
        "name": "foo_test",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "iama": "mixin",
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "idempotent": false,
          "value": "test"
        }
      }
    ]
  }
}
"""

LUCI_MILO_CFG = """\
consoles {
  builders {
    name: "buildbucket/luci.chromium.ci/Fake Tester"
  }
}
"""

LUCI_MILO_CFG_WATERFALL_SORTING = """\
consoles {
  builders {
    name: "buildbucket/luci.chromium.ci/Fake Tester"
    name: "buildbucket/luci.chromium.ci/Really Fake Tester"
  }
}
"""

TEST_SUITE_SORTING_WATERFALL = """
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
]
"""

TEST_SUITE_SORTED_WATERFALL = """
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
]
"""

TEST_SUITE_UNSORTED_WATERFALL_1 = """
[
  {
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
]
"""

TEST_SUITE_UNSORTED_WATERFALL_2 = """
[
  {
    'name': 'chromium.test',
    'machines': {
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'instrumentation_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
]
"""

# Note that the suites in basic_suites would be sorted after the suites in
# compound_suites. This is valid though, because each set of suites is sorted
# separately.
# suite_c is an 'instrumentation_tests' test
# suite_d is an 'scripts' test
TEST_SUITE_SORTED = """\
{
  'basic_suites': {
    'suite_c': {
      'suite_c': {},
    },
    'suite_d': {
      'script': {
        'script': 'suite_d.py',
      }
    },
  },
  'compound_suites': {
    'suite_a': [
      'suite_c',
    ],
    'suite_b': [
      'suite_d',
    ],
  },
}
"""

TEST_SUITE_UNSORTED_1 = """\
{
  'basic_suites': {
    'suite_d': {
      'a': 'b',
    },
    'suite_c': {
      'a': 'b',
    },
  },
  'compound_suites': {
    'suite_a': [
      'suite_c',
    ],
    'suite_b': [
      'suite_d',
    ],
  },
}
"""

TEST_SUITE_UNSORTED_2 = """\
{
  'basic_suites': {
    'suite_c': {
      'a': 'b',
    },
    'suite_d': {
      'a': 'b',
    },
  },
  'compound_suites': {
    'suite_b': [
      'suite_c',
    ],
    'suite_a': [
      'suite_d',
    ],
  },
}
"""
TEST_SUITE_UNSORTED_3 = """\
{
  'basic_suites': {
    'suite_d': {
      'a': 'b',
    },
    'suite_c': {
      'a': 'b',
    },
  },
  'compound_suites': {
    'suite_b': [
      'suite_c',
    ],
    'suite_a': [
      'suite_d',
    ],
  },
}
"""


TEST_SUITES_SYNTAX_ERROR = """\
{
  'basic_suites': {
    3: {
      'suite_c': {},
    },
  },
  'compound_suites': {},
}
"""

class UnitTest(unittest.TestCase):
  def test_base_generator(self):
    # Only needed for complete code coverage.
    self.assertRaises(NotImplementedError,
                      generate_buildbot_json.BaseGenerator(None).generate,
                      None, None, None, None)
    self.assertRaises(NotImplementedError,
                      generate_buildbot_json.BaseGenerator(None).sort,
                      None)

  def test_good_test_suites_are_ok(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_multi_dimension_test_suites_are_ok(self):
    fbb = FakeBBGen(FOO_GTESTS_MULTI_DIMENSION_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_composition_test_suites_are_ok(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_bad_composition_test_suites_are_caught(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    BAD_COMPOSITION_TEST_SUITES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 'Composition test suites may not refer to.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_composition_test_suites_no_duplicate_names(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    DUPLICATES_COMPOSITION_TEST_SUITES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 '.*may not duplicate basic test suite.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unknown_test_suites_are_caught(self):
    fbb = FakeBBGen(UNKNOWN_TEST_SUITE_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 'Test suite baz_tests from machine.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unknown_test_suite_types_are_caught(self):
    fbb = FakeBBGen(UNKNOWN_TEST_SUITE_TYPE_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 'Unknown test suite type foo_test_type.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unrefed_test_suite_caught(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    UNREFED_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 '.*unreferenced.*bar_tests.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_waterfall_output(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = COMPOSITION_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_reusing_gtest_targets(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = VARIATION_GTEST_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_noop_exception_does_nothing(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    EMPTY_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = COMPOSITION_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_arg_merges(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    FOO_TEST_MODIFICATIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = MERGED_ARGS_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_enable_features_arg_merges(self):
    fbb = FakeBBGen(FOO_GTESTS_WITH_ENABLE_FEATURES_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = MERGED_ENABLE_FEATURES_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_linux_args(self):
    fbb = FakeBBGen(FOO_LINUX_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_LINUX_ARGS,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = LINUX_ARGS_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_filtering(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = COMPOSITION_WATERFALL_FILTERED_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_modifications(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    FOO_TEST_MODIFICATIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = MODIFIED_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolated_script_tests(self):
    fbb = FakeBBGen(FOO_ISOLATED_SCRIPTS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = ISOLATED_SCRIPT_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script_with_args(self):
    fbb = FakeBBGen(FOO_SCRIPT_WATERFALL,
                    SCRIPT_SUITE,
                    SCRIPT_WITH_ARGS_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = SCRIPT_WITH_ARGS_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script(self):
    fbb = FakeBBGen(FOO_SCRIPT_WATERFALL,
                    FOO_SCRIPT_SUITE,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = SCRIPT_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_junit_tests(self):
    fbb = FakeBBGen(FOO_JUNIT_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = JUNIT_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_cts_tests(self):
    fbb = FakeBBGen(FOO_CTS_WATERFALL,
                    FOO_CTS_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = CTS_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_instrumentation_tests(self):
    fbb = FakeBBGen(FOO_INSTRUMENTATION_TEST_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = INSTRUMENTATION_TEST_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests(self):
    fbb = FakeBBGen(FOO_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = GPU_TELEMETRY_TEST_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_instrumentation_tests_with_different_names(self):
    fbb = FakeBBGen(FOO_INSTRUMENTATION_TEST_WATERFALL,
                    INSTRUMENTATION_TESTS_WITH_DIFFERENT_NAMES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = \
        INSTRUMENTATION_TEST_DIFFERENT_NAMES_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_ungenerated_output_files_are_caught(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    NO_BAR_TEST_EXCEPTIONS,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = (
      '\n' + COMPOSITION_WATERFALL_FILTERED_OUTPUT)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True, dump=False)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegexpMatches(
        joined_lines, 'Waterfall chromium.test did not have the following'
        ' expected contents:.*')
    self.assertRegexpMatches(joined_lines, '.*--- expected.*')
    self.assertRegexpMatches(joined_lines, '.*\+\+\+ current.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_android_output_options(self):
    fbb = FakeBBGen(ANDROID_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = ANDROID_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_removal_raises(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    NONEXISTENT_REMOVAL,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 'The following nonexistent machines.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_modification_raises(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    NONEXISTENT_MODIFICATION,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 'The following nonexistent machines.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfall_args(self):
    fbb = FakeBBGen(COMPOSITION_GTEST_SUITE_WITH_ARGS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = COMPOSITION_WATERFALL_WITH_ARGS_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_multi_dimension_output(self):
    fbb = FakeBBGen(FOO_GTESTS_MULTI_DIMENSION_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = MULTI_DIMENSION_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_relative_pyl_file_dir(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.args = argparse.Namespace(pyl_files_dir='relative/path/')
    for file_name in list(fbb.files):
      if not 'luci-milo.cfg' in file_name:
        fbb.files[os.path.join('relative/path/', file_name)] = (
            fbb.files.pop(file_name))
    fbb.check_input_file_consistency(verbose=True)
    fbb.files['relative/path/chromium.test.json'] = VARIATION_GTEST_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_bot_raises(self):
    fbb = FakeBBGen(UNKNOWN_BOT_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfalls_must_be_sorted(self):
    fbb = FakeBBGen(TEST_SUITE_SORTED_WATERFALL,
                    TEST_SUITE_SORTED,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG_WATERFALL_SORTING)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(TEST_SUITE_UNSORTED_WATERFALL_1,
                    TEST_SUITE_SORTED,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG_WATERFALL_SORTING)
    with self.assertRaisesRegexp(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: waterfalls.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegexpMatches(
      joined_lines, '.*\+chromium\..*test.*')
    self.assertRegexpMatches(
      joined_lines, '.*\-chromium\..*test.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(TEST_SUITE_UNSORTED_WATERFALL_2,
                    TEST_SUITE_SORTED,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG_WATERFALL_SORTING)
    with self.assertRaisesRegexp(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: waterfalls.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegexpMatches(
      joined_lines, '.*\+.*Fake Tester.*')
    self.assertRegexpMatches(
      joined_lines, '.*\-.*Fake Tester.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suite_exceptions_must_be_sorted(self):
    fbb = FakeBBGen(TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    EXCEPTIONS_SORTED,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    EXCEPTIONS_UNSORTED,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegexpMatches(
        joined_lines, '.*\+suite_.*')
    self.assertRegexpMatches(
        joined_lines, '.*\-suite_.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suites_must_be_sorted(self):
    fbb = FakeBBGen(TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    for unsorted in (
        TEST_SUITE_UNSORTED_1,
        TEST_SUITE_UNSORTED_2,
        TEST_SUITE_UNSORTED_3,
    ):
      fbb = FakeBBGen(TEST_SUITE_SORTING_WATERFALL,
                      unsorted,
                      EMPTY_PYL_FILE,
                      EMPTY_PYL_FILE,
                      LUCI_MILO_CFG)
      with self.assertRaises(generate_buildbot_json.BBGenErr):
        fbb.check_input_file_consistency(verbose=True)
      joined_lines = ' '.join(fbb.printed_lines)
      self.assertRegexpMatches(
          joined_lines, '.*\+suite_.*')
      self.assertRegexpMatches(
          joined_lines, '.*\-suite_.*')
      fbb.printed_lines = []
      self.assertFalse(fbb.printed_lines)


FOO_GTESTS_WATERFALL_MIXIN_WATERFALL = """\
[
  {
    'mixins': ['waterfall_mixin'],
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_BUILDER_MIXIN_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['builder_mixin'],
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_BUILDER_MIXIN_NON_SWARMING_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['random_mixin'],
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_DIMENSIONS_MIXIN_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['dimension_mixin'],
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GPU_TELEMETRY_TEST_DIMENSIONS_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['dimension_mixin'],
        'os_type': 'win',
        'browser_config': 'release',
        'test_suites': {
          'gpu_telemetry_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

# Swarming mixins must be a list, a single string is not allowed.
FOO_GTESTS_INVALID_LIST_MIXIN_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': 'dimension_mixin',
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""
FOO_GTESTS_INVALID_NOTFOUND_MIXIN_WATERFALL = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['nonexistant'],
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_TEST_MIXIN_WATERFALL = """\
[
  {
    'mixins': ['waterfall_mixin'],
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_SORTING_MIXINS_WATERFALL = """\
[
  {
    'mixins': ['a_mixin', 'b_mixin', 'c_mixin'],
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {
          'dimension_sets': [
            {
              'kvm': '1',
            },
          ],
        },
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_TEST_SUITE_WITH_MIXIN = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'dimension_sets': [
            {
              'integrity': 'high',
            }
          ],
          'expiration': 120,
        },
        'mixins': ['test_mixin'],
      },
    },
  },
}
"""

# These mixins are invalid; if passed to check_input_file_consistency, they will
# fail. These are used for output file consistency checks.
SWARMING_MIXINS = """\
{
  'builder_mixin': {
    'swarming': {
      'value': 'builder',
    },
  },
  'dimension_mixin': {
    'swarming': {
      'dimensions': {
        'iama': 'mixin',
      },
    },
  },
  'random_mixin': {
    'value': 'random',
  },
  'test_mixin': {
    'swarming': {
      'value': 'test',
    },
  },
  'waterfall_mixin': {
    'swarming': {
      'value': 'waterfall',
    },
  },
}
"""

SWARMING_MIXINS_DUPLICATED = """\
{
  'builder_mixin': {
    'value': 'builder',
  },
  'builder_mixin': {
    'value': 'builder',
  },
}
"""

SWARMING_MIXINS_UNSORTED = """\
{
  'b_mixin': {
    'b': 'b',
  },
  'a_mixin': {
    'a': 'a',
  },
  'c_mixin': {
    'c': 'c',
  },
}
"""

SWARMING_MIXINS_SORTED = """\
{
  'a_mixin': {
    'a': 'a',
  },
  'b_mixin': {
    'b': 'b',
  },
  'c_mixin': {
    'c': 'c',
  },
}
"""

FOO_CTS_WATERFALL_MIXINS = """\
[
  {
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['test_mixin'],
        'test_suites': {
          'cts_tests': 'foo_cts_tests',
        },
      },
    },
  },
]
"""

WATERFALL_MIXIN_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "value": "waterfall"
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

WATERFALL_MIXIN_WATERFALL_EXCEPTION_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "value": "exception"
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

BUILDER_MIXIN_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "value": "builder"
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

BUILDER_MIXIN_NON_SWARMING_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high"
            }
          ],
          "expiration": 120
        },
        "test": "foo_test",
        "value": "random"
      }
    ]
  }
}
"""

TEST_MIXIN_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high",
              "kvm": "1"
            }
          ],
          "expiration": 120,
          "value": "test"
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

DIMENSIONS_MIXIN_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "iama": "mixin",
              "integrity": "high"
            }
          ],
          "expiration": 120,
          "value": "test"
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

class MixinTests(unittest.TestCase):
  """Tests for the mixins feature."""
  def test_mixins_must_be_sorted(self):
    fbb = FakeBBGen(FOO_GTESTS_SORTING_MIXINS_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS_SORTED,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(FOO_GTESTS_SORTING_MIXINS_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS_UNSORTED,
                    LUCI_MILO_CFG)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegexpMatches(
        joined_lines, '.*\+._mixin.*')
    self.assertRegexpMatches(
        joined_lines, '.*\-._mixin.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_waterfall(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = WATERFALL_MIXIN_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfall_exception_overrides(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    SCRIPT_WITH_ARGS_SWARMING_EXCEPTIONS,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = WATERFALL_MIXIN_WATERFALL_EXCEPTION_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder(self):
    fbb = FakeBBGen(FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = BUILDER_MIXIN_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder_non_swarming(self):
    fbb = FakeBBGen(FOO_GTESTS_BUILDER_MIXIN_NON_SWARMING_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = (
        BUILDER_MIXIN_NON_SWARMING_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_suite(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = TEST_MIXIN_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension(self):
    fbb = FakeBBGen(FOO_GTESTS_DIMENSIONS_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = DIMENSIONS_MIXIN_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension_gpu(self):
    fbb = FakeBBGen(FOO_GPU_TELEMETRY_TEST_DIMENSIONS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = GPU_DIMENSIONS_WATERFALL_OUTPUT
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unreferenced(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(generate_buildbot_json.BBGenErr,
                                 '.*mixins are unreferenced.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_cts(self):
    fbb = FakeBBGen(FOO_CTS_WATERFALL_MIXINS,
                    FOO_CTS_SUITE ,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = CTS_OUTPUT
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unused(self):
    fbb = FakeBBGen(FOO_GTESTS_INVALID_NOTFOUND_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = DIMENSIONS_MIXIN_WATERFALL_OUTPUT
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_list(self):
    fbb = FakeBBGen(FOO_GTESTS_INVALID_LIST_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS,
                    LUCI_MILO_CFG)
    fbb.files['chromium.test.json'] = DIMENSIONS_MIXIN_WATERFALL_OUTPUT
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)


  def test_no_duplicate_keys(self):
    fbb = FakeBBGen(FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    EMPTY_PYL_FILE,
                    SWARMING_MIXINS_DUPLICATED,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: mixins.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegexpMatches(
        joined_lines, 'Key .* is duplicated')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_type_assert_printing_help(self):
    fbb = FakeBBGen(FOO_GTESTS_WATERFALL,
                    TEST_SUITES_SYNTAX_ERROR,
                    EMPTY_PYL_FILE,
                    EMPTY_PYL_FILE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegexp(
        generate_buildbot_json.BBGenErr,
        'Invalid \.pyl file \'test_suites.pyl\'.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertEquals(
        fbb.printed_lines, [
          '== test_suites.pyl ==',
          '<snip>',
          '1 {',
          "2   'basic_suites': {",
          '--------------------------------------------------------------------'
          '------------',
          '3     3: {',
          '-------^------------------------------------------------------------'
          '------------',
          "4       'suite_c': {},",
          '5     },',
          '<snip>',
        ])


if __name__ == '__main__':
  unittest.main()

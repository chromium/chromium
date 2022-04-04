#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_buildbot_json.py."""

import argparse
import contextlib
import json
import os
import unittest

import generate_buildbot_json
from pyfakefs import fake_filesystem_unittest

# pylint: disable=super-with-arguments

EMPTY_PYL_FILE = """\
{
}
"""

# Use this value to refer to the directory containing this code
# The tests use a fake filesystem and python filesystem calls are monkey-patched
# to use the fake filesystem, which affects abspath
THIS_DIR = os.path.dirname(os.path.abspath(__file__))


class TestCase(fake_filesystem_unittest.TestCase):
  def setUp(self):
    self.setUpPyfakefs()
    self.fs.cwd = THIS_DIR
    self.args = generate_buildbot_json.BBJSONGenerator.parse_args([])

  def override_args(self, **kwargs):
    for k, v in kwargs.items():
      setattr(self.args, k, v)

  def create_testing_buildbot_json_file(self, path, contents):
    return self.fs.create_file(os.path.join(THIS_DIR, path), contents=contents)


@contextlib.contextmanager
def dump_on_failure(fbb, dump=True):
  try:
    yield
  except:
    if dump:
      for l in fbb.printed_lines:
        print(l)
    raise

class FakeBBGen(generate_buildbot_json.BBJSONGenerator):
  def __init__(self,
               args,
               waterfalls,
               test_suites,
               luci_milo_cfg,
               project_pyl='{"validate_source_side_specs_have_builder": True}',
               exceptions=EMPTY_PYL_FILE,
               mixins=EMPTY_PYL_FILE,
               gn_isolate_map=EMPTY_PYL_FILE,
               variants=EMPTY_PYL_FILE):
    super(FakeBBGen, self).__init__(args)

    pyl_files_dir = args.pyl_files_dir or THIS_DIR
    infra_config_dir = args.infra_config_dir

    files = {
        (pyl_files_dir, 'waterfalls.pyl'): waterfalls,
        (pyl_files_dir, 'test_suites.pyl'): test_suites,
        (pyl_files_dir, 'test_suite_exceptions.pyl'): exceptions,
        (pyl_files_dir, 'mixins.pyl'): mixins,
        (pyl_files_dir, 'gn_isolate_map.pyl'): gn_isolate_map,
        (pyl_files_dir, 'gn_isolate_map2.pyl'): GPU_TELEMETRY_GN_ISOLATE_MAP,
        (pyl_files_dir, 'variants.pyl'): variants,
        (infra_config_dir, 'generated/project.pyl'): project_pyl,
        (infra_config_dir, 'generated/luci/luci-milo.cfg'): luci_milo_cfg,
        (infra_config_dir, 'generated/luci/luci-milo-dev.cfg'): '',
    }
    for (d, filename), content in files.items():
      if content is None:
        continue
      path = os.path.join(d, filename)
      parent = os.path.abspath(os.path.dirname(path))
      if not os.path.exists(parent):
        os.makedirs(parent)
      with open(path, 'w') as f:
        f.write(content)

    self.printed_lines = []

  def print_line(self, line):
    self.printed_lines.append(line)

  # pragma pylint: disable=arguments-differ
  def check_output_file_consistency(self, verbose=False, dump=True):
    with dump_on_failure(self, dump=verbose and dump):
      super(FakeBBGen, self).check_output_file_consistency(verbose)
  # pragma pylint: enable=arguments-differ


FOO_GTESTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_CHROMEOS_TRIGGER_SCRIPT_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'swarming': {
          'dimension_sets': [
            {
              "device_type": "foo_device",
            },
          ],
        },
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
        'os_type': 'chromeos',
      },
    },
  },
]
"""

FOO_LINUX_GTESTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_ISOLATED_SCRIPTS_WATERFALL_ANDROID = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'android',
        'test_suites': {
          'isolated_scripts': 'composition_tests',
        },
        'use_android_presentation': True,
      },
    },
  },
]
"""

FOO_SCRIPT_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_SCRIPT_WATERFALL_MACHINE_FORBIDS_SCRIPT_TESTS = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'forbid_script_tests': True,
        'test_suites': {
          'scripts': 'foo_scripts',
        },
      },
    },
  },
]
"""

FOO_SCRIPT_WATERFALL_FORBID_SCRIPT_TESTS = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'forbid_script_tests': True,
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
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_GPU_TELEMETRY_TEST_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'android',
        'browser_config': 'android-chromium',
        'swarming': {
          'dimension_sets': [
            {
              'device_type': 'bullhead',
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

FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID_WEBVIEW = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'android',
        'browser_config': 'not-a-real-browser',
        'swarming': {
          'dimension_sets': [
            {
              'device_type': 'bullhead',
            },
          ],
        },
        'test_suites': {
          'android_webview_gpu_telemetry_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_GPU_TELEMETRY_TEST_WATERFALL_FUCHSIA = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'fuchsia',
        'browser_config': 'fuchsia-chrome',
        'swarming': {
          'dimension_sets': [
            {
              'kvm': '1',
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

NVIDIA_GPU_TELEMETRY_TEST_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'win',
        'browser_config': 'release',
        'swarming': {
          'dimension_sets': [
            {
              'gpu': '10de:1cb3-26.21.14.3102',
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

INTEL_GPU_TELEMETRY_TEST_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'win',
        'browser_config': 'release',
        'swarming': {
          'dimension_sets': [
            {
              'gpu': '8086:5912-24.20.100.6286',
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

INTEL_UHD_GPU_TELEMETRY_TEST_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'win',
        'browser_config': 'release',
        'swarming': {
          'dimension_sets': [
            {
              'gpu': '8086:3e92-24.20.100.6286',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
              'os': 'Android',
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
              'device_os': 'LMY41U',
              'device_os_type': 'user',
              'device_type': 'hammerhead',
              'os': 'Android',
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
              'os': 'Android',
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
    'project': 'chromium',
    'bucket': 'ci',
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

MATRIX_GTEST_SUITE_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'matrix_tests',
        },
      },
    },
  },
]
"""

MATRIX_GTEST_SUITE_WATERFALL_MIXINS = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': ['dimension_mixin'],
        'test_suites': {
          'gtest_tests': 'matrix_tests',
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

FOO_TEST_SUITE_NO_DIMENSIONS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
      },
    },
  },
}
"""

FOO_TEST_SUITE_NOT_SORTED = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
      'a_test': {},
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

FOO_TEST_SUITE_WITH_SWARMING_NAMED_CACHES = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'named_caches': [
            {
              'name': 'cache_in_test',
              'file': 'cache_in_test_file',
            },
          ],
        },
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

FOO_TEST_SUITE_WITH_REMOVE_WATERFALL_MIXIN = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'remove_mixins': ['waterfall_mixin'],
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

FOO_TEST_SUITE_WITH_REMOVE_BUILDER_MIXIN = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'remove_mixins': ['builder_mixin'],
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

CONFLICTING_COMPOSITION_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {
      'baz_tests': {
        'args': [
          '--bar',
        ],
      }
    },
    'foo_tests': {
      'baz_tests': {
        'args': [
          '--foo',
        ],
      }
    },
  },
  'compound_suites': {
    'foobar_tests': [
      'foo_tests',
      'bar_tests',
    ],
  },
}
"""

DUPLICATES_COMPOSITION_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {},
    'buggy_composition_tests': {},
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

COMPOSITION_SUITE_WITH_GPU_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo': {
        'args': [
          '--gpu-vendor-id',
          '${gpu_vendor_id}',
          '--gpu-device-id',
          '${gpu_device_id}',
        ],
      },
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

COMPOSITION_SUITE_WITH_PIXEL_AND_FILTER = """\
{
  'basic_suites': {
    'foo_tests': {
      'pixel': {
        'args': [
          '--git-revision=aaaa',
          '--test-filter=bar',
        ],
      },
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

GTEST_AS_ISOLATED_SCRIPT_SUITE = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'script': 'foo.py',
        'use_isolated_scripts_api': True,
      },
    },
  },
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

EXCEPTIONS_PER_TEST_UNSORTED = """\
{
  'suite_d': {
    'modifications': {
      'Other Tester': {
        'foo': 'baz',
      },
      'Fake Tester': {
        'foo': 'baz',
      },
    },
  },
}
"""

EXCEPTIONS_DUPS_REMOVE_FROM = """\
{
  'suite_d': {
    'remove_from': [
      'Fake Tester',
      'Fake Tester',
    ],
    'modifications': {
      'Fake Tester': {
        'foo': 'baz',
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

FOO_TEST_EXPLICIT_NONE_EXCEPTIONS = """\
{
  'foo_test': {
    'modifications': {
      'Fake Tester': {
        'swarming': {
          'dimension_sets': [
            {
              'integrity': None,
            },
          ],
        },
      },
    },
  },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "bar_test"
      },
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "bar_test"
      },
      {
        "args": [
          "--this-is-an-argument"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ]
        },
        "test": "foo_test",
        "test_id_prefix": "ninja://chrome/test:foo_test/"
      },
      {
        "args": [
          "--variation"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "variation_test",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ]
        },
        "test": "foo_test",
        "test_id_prefix": "ninja://chrome/test:foo_test/"
      }
    ]
  }
}
"""

FOO_WATERFALL_GTEST_ISOLATED_SCRIPT_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "script": "foo.py",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ]
        },
        "test": "foo_test",
        "test_id_prefix": "ninja://chrome/test:foo_test/",
        "use_isolated_scripts_api": true
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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

EXPLICIT_NONE_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ],
          "expiration": 120
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_test",
        "swarming": {
          "can_use_on_swarming_builders": true
        }
      }
    ]
  }
}
"""

ISOLATED_SCRIPT_OUTPUT_ANDROID = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "--gs-results-bucket=chromium-result-details"
        ],
        "isolate_name": "foo_test",
        "merge": {
          "args": [
            "--bucket",
            "chromium-result-details",
            "--test-name",
            "foo_test"
          ],
          "script": \
"//build/android/pylib/results/presentation/test_results_presentation.py"
        },
        "name": "foo_test",
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
        "name": "foo_test",
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "10de:1cb3"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test/"
      }
    ]
  }
}
"""

GPU_TELEMETRY_TEST_OUTPUT_ANDROID = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "foo",
          "--show-stdout",
          "--browser=android-chromium",
          "--passthrough",
          "-v",
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc"
        ],
        "isolate_name": "telemetry_gpu_integration_test_android_chrome",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets": [
            {
              "device_type": "bullhead"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test_android_chrome/"
      }
    ]
  }
}
"""

GPU_TELEMETRY_TEST_OUTPUT_ANDROID_WEBVIEW = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "foo",
          "--show-stdout",
          "--browser=android-webview-instrumentation",
          "--passthrough",
          "-v",
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc"
        ],
        "isolate_name": "telemetry_gpu_integration_test_android_webview",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets": [
            {
              "device_type": "bullhead"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test_android_webview/"
      }
    ]
  }
}
"""

GPU_TELEMETRY_TEST_OUTPUT_FUCHSIA = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "isolated_scripts": [
      {
        "args": [
          "foo",
          "--show-stdout",
          "--browser=fuchsia-chrome",
          "--passthrough",
          "-v",
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc"
        ],
        "isolate_name": "telemetry_gpu_integration_test_fuchsia",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "kvm": "1"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test_fuchsia/"
      }
    ]
  }
}
"""

NVIDIA_GPU_TELEMETRY_TEST_OUTPUT = """\
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
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc",
          "--gpu-vendor-id",
          "10de",
          "--gpu-device-id",
          "1cb3"
        ],
        "isolate_name": "telemetry_gpu_integration_test",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "10de:1cb3-26.21.14.3102"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test/"
      }
    ]
  }
}
"""

INTEL_GPU_TELEMETRY_TEST_OUTPUT = """\
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
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc",
          "--gpu-vendor-id",
          "8086",
          "--gpu-device-id",
          "5912"
        ],
        "isolate_name": "telemetry_gpu_integration_test",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "8086:5912-24.20.100.6286"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test/"
      }
    ]
  }
}
"""

INTEL_UHD_GPU_TELEMETRY_TEST_OUTPUT = """\
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
          "--extra-browser-args=--enable-logging=stderr --js-flags=--expose-gc",
          "--gpu-vendor-id",
          "8086",
          "--gpu-device-id",
          "3e92"
        ],
        "isolate_name": "telemetry_gpu_integration_test",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_tests",
        "should_retry_with_patch": false,
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "gpu": "8086:3e92-24.20.100.6286"
            }
          ],
          "idempotent": false
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test/"
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
              "device_os_type": "userdebug",
              "device_type": "hammerhead",
              "integrity": "high",
              "os": "Android"
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
              "device_os": "LMY41U",
              "device_os_type": "user",
              "device_type": "hammerhead",
              "integrity": "high",
              "os": "Android"
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": false
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

CHROMEOS_TRIGGER_SCRIPT_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "device_type": "foo_device",
              "integrity": "high"
            }
          ],
          "expiration": 120
        },
        "test": "foo_test",
        "trigger_script": {
          "script": "//testing/trigger_scripts/chromeos_device_trigger.py"
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_test",
        "should_retry_with_patch": false,
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
        },
        "test_id_prefix": "ninja://chrome/test:telemetry_gpu_integration_test/"
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
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
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
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
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
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
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
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
    },
  },
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.zz.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
          'scripts': 'suite_b',
        },
      },
      'Really Fake Tester': {
        'test_suites': {
          'gtest_tests': 'suite_a',
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
# suite_c is a 'gtest_tests' test
# suite_d is a 'scripts' test
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

GN_ISOLATE_MAP="""\
{
  'foo_test': {
    'label': '//chrome/test:foo_test',
    'type': 'windowed_test_launcher',
  }
}
"""

GPU_TELEMETRY_GN_ISOLATE_MAP="""\
{
  'telemetry_gpu_integration_test': {
    'label': '//chrome/test:telemetry_gpu_integration_test',
    'type': 'script',
      }
}
"""

GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID = """\
{
  'telemetry_gpu_integration_test_android_chrome': {
    'label': '//chrome/test:telemetry_gpu_integration_test_android_chrome',
    'type': 'script',
      }
}
"""

GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID_WEBVIEW = """\
{
  'telemetry_gpu_integration_test_android_webview': {
    'label': '//chrome/test:telemetry_gpu_integration_test_android_webview',
    'type': 'script',
      }
}
"""

GPU_TELEMETRY_GN_ISOLATE_MAP_FUCHSIA = """\
{
  'telemetry_gpu_integration_test_fuchsia': {
    'label': '//chrome/test:telemetry_gpu_integration_test_fuchsia',
    'type': 'script',
      }
}
"""

GN_ISOLATE_MAP_KEY_LABEL_MISMATCH="""\
{
  'foo_test': {
    'label': '//chrome/test:foo_test_tmp',
    'type': 'windowed_test_launcher',
  }
}
"""

GN_ISOLATE_MAP_USING_IMPLICIT_NAME="""\
{
  'foo_test': {
    'label': '//chrome/foo_test',
    'type': 'windowed_test_launcher',
  }
}
"""

class UnitTest(TestCase):
  def test_base_generator(self):
    # Only needed for complete code coverage.
    self.assertRaises(NotImplementedError,
                      generate_buildbot_json.BaseGenerator(None).generate,
                      None, None, None, None)
    self.assertRaises(NotImplementedError,
                      generate_buildbot_json.BaseGenerator(None).sort,
                      None)

  def test_good_test_suites_are_ok(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, FOO_TEST_SUITE,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_composition_test_suites_are_ok(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_bad_composition_test_suites_are_caught(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WATERFALL,
                    BAD_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'compound_suites may not refer to.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_composition_test_suites_no_conflicts(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WATERFALL,
                    CONFLICTING_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Conflicting test definitions.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_composition_test_suites_no_duplicate_names(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WATERFALL,
                    DUPLICATES_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                '.*may not duplicate basic test suite.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unknown_test_suites_are_caught(self):
    fbb = FakeBBGen(self.args, UNKNOWN_TEST_SUITE_WATERFALL, FOO_TEST_SUITE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Test suite baz_tests from machine.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unknown_test_suite_types_are_caught(self):
    fbb = FakeBBGen(self.args, UNKNOWN_TEST_SUITE_TYPE_WATERFALL,
                    FOO_TEST_SUITE, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Unknown test suite type foo_test_type.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unrefed_test_suite_caught(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, UNREFED_TEST_SUITE,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                '.*unreferenced.*bar_tests.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_waterfall_output(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           COMPOSITION_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_reusing_gtest_targets(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           VARIATION_GTEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_load_multiple_isolate_map_files_with_duplicates(self):
    self.args.isolate_map_files = ['gn_isolate_map.pyl']
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Duplicate targets in isolate map files.*'):
      fbb.load_configuration_files()

  def test_load_multiple_isolate_map_files_without_duplicates(self):
    self.args.isolate_map_files = ['gn_isolate_map2.pyl']
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    fbb.load_configuration_files()
    isolate_dict = {}
    isolate_map_1 = fbb.load_pyl_file('gn_isolate_map.pyl')
    isolate_map_2 = fbb.load_pyl_file('gn_isolate_map2.pyl')
    isolate_dict.update(isolate_map_1)
    isolate_dict.update(isolate_map_2)
    self.assertEqual(isolate_dict, fbb.gn_isolate_map)

  def test_gn_isolate_map_with_label_mismatch(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP_KEY_LABEL_MISMATCH)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'key name.*foo_test.*label.*'
                                'foo_test_tmp.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gn_isolate_map_using_implicit_gn_name(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP_USING_IMPLICIT_NAME)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Malformed.*//chrome/foo_test.*for key.*'
        'foo_test.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_noop_exception_does_nothing(self):
    fbb = FakeBBGen(self.args,
                    COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           COMPOSITION_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_arg_merges(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_MODIFICATIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MERGED_ARGS_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_enable_features_arg_merges(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WITH_ENABLE_FEATURES_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MERGED_ENABLE_FEATURES_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_linux_args(self):
    fbb = FakeBBGen(self.args, FOO_LINUX_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_LINUX_ARGS, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           LINUX_ARGS_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_filtering(self):
    fbb = FakeBBGen(self.args,
                    COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', COMPOSITION_WATERFALL_FILTERED_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_modifications(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_MODIFICATIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MODIFIED_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json', MODIFIED_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_with_explicit_none(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_EXPLICIT_NONE_EXCEPTIONS,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           EXPLICIT_NONE_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           EXPLICIT_NONE_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolated_script_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           ISOLATED_SCRIPT_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           ISOLATED_SCRIPT_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolated_script_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL_ANDROID,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           ISOLATED_SCRIPT_OUTPUT_ANDROID)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           ISOLATED_SCRIPT_OUTPUT_ANDROID)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script_with_args(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL,
                    SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=SCRIPT_WITH_ARGS_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           SCRIPT_WITH_ARGS_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           SCRIPT_WITH_ARGS_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL,
                    FOO_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json', SCRIPT_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json', SCRIPT_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script_machine_forbids_scripts(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL_MACHINE_FORBIDS_SCRIPT_TESTS,
                    FOO_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Attempted to generate a script test on tester.*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_script_waterfall_forbids_scripts(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL_FORBID_SCRIPT_TESTS,
                    FOO_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Attempted to generate a script test on tester.*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_junit_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_JUNIT_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json', JUNIT_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json', JUNIT_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           GPU_TELEMETRY_TEST_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           GPU_TELEMETRY_TEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_pixel_with_filter(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_PIXEL_AND_FILTER,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           GPU_TELEMETRY_TEST_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           GPU_TELEMETRY_TEST_OUTPUT)
    with self.assertRaises(RuntimeError):
      fbb.check_output_file_consistency(verbose=True)

  def test_gpu_telemetry_tests_android(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID,
                    COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           GPU_TELEMETRY_TEST_OUTPUT_ANDROID)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           GPU_TELEMETRY_TEST_OUTPUT_ANDROID)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_android_webview(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID_WEBVIEW,
                    COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID_WEBVIEW)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', GPU_TELEMETRY_TEST_OUTPUT_ANDROID_WEBVIEW)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', GPU_TELEMETRY_TEST_OUTPUT_ANDROID_WEBVIEW)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_fuchsia(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_FUCHSIA,
                    COMPOSITION_SUITE_WITH_NAME_NOT_ENDING_IN_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_FUCHSIA)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           GPU_TELEMETRY_TEST_OUTPUT_FUCHSIA)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           GPU_TELEMETRY_TEST_OUTPUT_FUCHSIA)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nvidia_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    NVIDIA_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           NVIDIA_GPU_TELEMETRY_TEST_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           NVIDIA_GPU_TELEMETRY_TEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_intel_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    INTEL_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           INTEL_GPU_TELEMETRY_TEST_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           INTEL_GPU_TELEMETRY_TEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_intel_uhd_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    INTEL_UHD_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           INTEL_UHD_GPU_TELEMETRY_TEST_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           INTEL_UHD_GPU_TELEMETRY_TEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gtest_as_isolated_Script(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    GTEST_AS_ISOLATED_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', FOO_WATERFALL_GTEST_ISOLATED_SCRIPT_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', FOO_WATERFALL_GTEST_ISOLATED_SCRIPT_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_ungenerated_output_files_are_caught(self):
    fbb = FakeBBGen(self.args,
                    COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', '\n' + COMPOSITION_WATERFALL_FILTERED_OUTPUT)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True, dump=False)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(
        joined_lines, 'File chromium.test.json did not have the following'
        ' expected contents:.*')
    self.assertRegex(joined_lines, '.*--- expected.*')
    self.assertRegex(joined_lines, '.*\+\+\+ current.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_android_output_options(self):
    fbb = FakeBBGen(self.args, ANDROID_WATERFALL, FOO_TEST_SUITE, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           ANDROID_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           ANDROID_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_removal_raises(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NONEXISTENT_REMOVAL)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'The following nonexistent machines.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_modification_raises(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NONEXISTENT_MODIFICATION)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'The following nonexistent machines.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfall_args(self):
    fbb = FakeBBGen(self.args, COMPOSITION_GTEST_SUITE_WITH_ARGS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', COMPOSITION_WATERFALL_WITH_ARGS_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', COMPOSITION_WATERFALL_WITH_ARGS_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_chromeos_trigger_script_output(self):
    fbb = FakeBBGen(self.args, FOO_CHROMEOS_TRIGGER_SCRIPT_WATERFALL,
                    FOO_TEST_SUITE, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           CHROMEOS_TRIGGER_SCRIPT_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           CHROMEOS_TRIGGER_SCRIPT_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_relative_pyl_file_dir(self):
    self.override_args(pyl_files_dir='relative/path/', waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    fbb.check_input_file_consistency(verbose=True)
    self.create_testing_buildbot_json_file('relative/path/chromium.test.json',
                                           VARIATION_GTEST_OUTPUT)
    self.create_testing_buildbot_json_file('relative/path/chromium.ci.json',
                                           VARIATION_GTEST_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_bot_raises(self):
    fbb = FakeBBGen(self.args, UNKNOWN_BOT_GTESTS_WATERFALL, FOO_TEST_SUITE,
                    LUCI_MILO_CFG)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_bot_raises_when_no_project_pyl_exists(self):
    fbb = FakeBBGen(self.args,
                    UNKNOWN_BOT_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    project_pyl=None)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nonexistent_bot_does_not_raise_when_validation_disabled(self):
    fbb = FakeBBGen(
        self.args,
        UNKNOWN_BOT_GTESTS_WATERFALL,
        FOO_TEST_SUITE,
        LUCI_MILO_CFG,
        project_pyl='{"validate_source_side_specs_have_builder": False}')
    fbb.check_input_file_consistency(verbose=True)

  def test_waterfalls_must_be_sorted(self):
    fbb = FakeBBGen(self.args, TEST_SUITE_SORTED_WATERFALL, TEST_SUITE_SORTED,
                    LUCI_MILO_CFG_WATERFALL_SORTING)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args, TEST_SUITE_UNSORTED_WATERFALL_1,
                    TEST_SUITE_SORTED, LUCI_MILO_CFG_WATERFALL_SORTING)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: waterfalls.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\+ chromium\..*test.*')
    self.assertRegex(joined_lines, '.*\- chromium\..*test.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args, TEST_SUITE_UNSORTED_WATERFALL_2,
                    TEST_SUITE_SORTED, LUCI_MILO_CFG_WATERFALL_SORTING)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: waterfalls.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\+.*Fake Tester.*')
    self.assertRegex(joined_lines, '.*\-.*Fake Tester.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suite_exceptions_must_be_sorted(self):
    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_SORTED)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_DUPS_REMOVE_FROM)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\- Fake Tester.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suite_exceptions_no_dups_remove_from(self):
    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_SORTED)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_PER_TEST_UNSORTED)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\+ Fake Tester.*')
    self.assertRegex(joined_lines, '.*\- Fake Tester.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suite_exceptions_per_test_must_be_sorted(self):
    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_SORTED)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args,
                    TEST_SUITE_SORTING_WATERFALL,
                    TEST_SUITE_SORTED,
                    LUCI_MILO_CFG,
                    exceptions=EXCEPTIONS_UNSORTED)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\+ suite_.*')
    self.assertRegex(joined_lines, '.*\- suite_.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_test_suites_must_be_sorted(self):
    fbb = FakeBBGen(self.args, TEST_SUITE_SORTING_WATERFALL, TEST_SUITE_SORTED,
                    LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    for unsorted in (
        TEST_SUITE_UNSORTED_1,
        TEST_SUITE_UNSORTED_2,
        TEST_SUITE_UNSORTED_3,
    ):
      fbb = FakeBBGen(self.args, TEST_SUITE_SORTING_WATERFALL, unsorted,
                      LUCI_MILO_CFG)
      with self.assertRaises(generate_buildbot_json.BBGenErr):
        fbb.check_input_file_consistency(verbose=True)
      joined_lines = ' '.join(fbb.printed_lines)
      self.assertRegex(joined_lines, '.*\+ suite_.*')
      self.assertRegex(joined_lines, '.*\- suite_.*')
      fbb.printed_lines = []
      self.assertFalse(fbb.printed_lines)


FOO_GTESTS_WATERFALL_MIXIN_WATERFALL = """\
[
  {
    'mixins': ['waterfall_mixin'],
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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

FOO_LINUX_GTESTS_BUILDER_MIXIN_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'linux',
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

FOO_GTESTS_DIMENSION_SETS_MIXIN_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'mixins': [
          'dimension_set_mixin_1',
          'dimension_set_mixin_2',
          'duplicate_dimension_set_mixin_1',
          'dimension_mixin',
        ],
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

FOO_GTESTS_WATERFALL_MIXIN_BUILDER_REMOVE_MIXIN_WATERFALL = """\
[
  {
    'mixins': ['waterfall_mixin'],
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'remove_mixins': ['waterfall_mixin'],
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
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
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'mixins': ['waterfall_mixin'],
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
    'project': 'chromium',
    'bucket': 'ci',
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

SWARMING_MIXINS_APPEND = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'args': [ '--mixin-argument' ],
      'linux_args': [ '--linux-mixin-argument' ],
    },
  },
}
"""

SWARMING_MIXINS_APPEND_NOT_LIST = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'args': 'I am not a list',
    },
  },
}
"""

SWARMING_MIXINS_APPEND_TO_SWARMING = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'swarming': [ 'swarming!' ],
    },
  },
}
"""

SWARMING_MIXINS_APPEND_NAMED_CACHES = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'swarming': {
        'named_caches': [
          {
            'name': 'cache',
            'file': 'cache_file',
          },
        ]
      },
    },
  },
}
"""

SWARMING_MIXINS_APPEND_OTHER_KEYS_WITH_NAMED_CACHES = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'swarming': {
        'named_caches': [
          {
            'name': 'cache',
            'file': 'cache_file',
          },
        ],
        'other_key': 'some value',
      },
    },
  },
}
"""

SWARMING_MIXINS_DIMENSION_SETS = """\
{
  'dimension_set_mixin_1': {
    'swarming': {
      'dimension_sets': [
        {
          'value': 'ds1',
        },
      ],
    },
  },
  'dimension_set_mixin_2': {
    'swarming': {
      'dimension_sets': [
        {
          'value': 'ds2',
        },
      ],
    },
  },
  'duplicate_dimension_set_mixin_1': {
    'swarming': {
      'dimension_sets': [
        {
          'value': 'ds1',
        },
      ],
    },
  },
  'dimension_mixin': {
    'swarming': {
      'dimensions': {
        'other_value': 'dimension_mixin',
      },
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

WATERFALL_DIMENSION_SETS_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "other_value": "dimension_mixin",
              "value": "ds1"
            },
            {
              "other_value": "dimension_mixin",
              "value": "ds2"
            }
          ]
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

WATERFALL_MIXIN_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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

WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "integrity": "high"
            }
          ],
          "expiration": 120
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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

BUILDER_MIXIN_APPEND_ARGS_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--c_arg",
          "--mixin-argument"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

BUILDER_MIXIN_APPEND_ARGS_LINUX_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--c_arg",
          "--mixin-argument",
          "--linux-mixin-argument"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

BUILDER_MIXIN_APPEND_NAMED_CACHES_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true,
          "named_caches": [
            {
              "file": "cache_in_test_file",
              "name": "cache_in_test"
            },
            {
              "file": "cache_file",
              "name": "cache"
            }
          ]
        },
        "test": "foo_test"
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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


class MixinTests(TestCase):
  """Tests for the mixins feature."""
  def test_mixins_must_be_sorted(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_SORTING_MIXINS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_SORTING_MIXINS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_UNSORTED)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\+ ._mixin.*')
    self.assertRegex(joined_lines, '.*\- ._mixin.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_waterfall(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           WATERFALL_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           WATERFALL_MIXIN_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfall_exception_overrides(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=SCRIPT_WITH_ARGS_SWARMING_EXCEPTIONS,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', WATERFALL_MIXIN_WATERFALL_EXCEPTION_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', WATERFALL_MIXIN_WATERFALL_EXCEPTION_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           BUILDER_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           BUILDER_MIXIN_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder_non_swarming(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_NON_SWARMING_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', BUILDER_MIXIN_NON_SWARMING_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', BUILDER_MIXIN_NON_SWARMING_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_suite(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           TEST_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           TEST_MIXIN_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_DIMENSIONS_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension_gpu(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_DIMENSIONS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           GPU_DIMENSIONS_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           GPU_DIMENSIONS_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unreferenced(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                '.*mixins are unreferenced.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_unused(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_INVALID_NOTFOUND_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_list(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_INVALID_LIST_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           DIMENSIONS_MIXIN_WATERFALL_OUTPUT)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)


  def test_no_duplicate_keys(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_DUPLICATED)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: mixins.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\- builder_mixin')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_no_duplicate_keys_basic_test_suite(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, FOO_TEST_SUITE_NOT_SORTED,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'The following files have invalid keys: test_suites.pyl'):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, '.*\- a_test')
    self.assertRegex(joined_lines, '.*\+ a_test')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_type_assert_printing_help(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, TEST_SUITES_SYNTAX_ERROR,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Invalid \.pyl file \'test_suites.pyl\'.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertEqual(fbb.printed_lines, [
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

  def test_mixin_append_args(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', BUILDER_MIXIN_APPEND_ARGS_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', BUILDER_MIXIN_APPEND_ARGS_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mixin_append_linux_args(self):
    fbb = FakeBBGen(self.args,
                    FOO_LINUX_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', BUILDER_MIXIN_APPEND_ARGS_LINUX_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', BUILDER_MIXIN_APPEND_ARGS_LINUX_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mixin_append_swarming_named_caches(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_SWARMING_NAMED_CACHES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND_NAMED_CACHES)
    self.create_testing_buildbot_json_file(
        'chromium.test.json',
        BUILDER_MIXIN_APPEND_NAMED_CACHES_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', BUILDER_MIXIN_APPEND_NAMED_CACHES_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mixin_append_swarming_error(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND_OTHER_KEYS_WITH_NAMED_CACHES)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Only named_caches is supported under swarming key in '
        '\$mixin_append, but there are: \[\'named_caches\', \'other_key\'\]'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mixin_append_mixin_field_not_list(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND_NOT_LIST)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Key "args" in \$mixin_append must be a list.'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mixin_append_test_field_not_list(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_APPEND_TO_SWARMING)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Cannot apply \$mixin_append to non-list "swarming".'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_remove_mixin_builder_remove_waterfall(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_BUILDER_REMOVE_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_remove_mixin_test_remove_waterfall(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_REMOVE_WATERFALL_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_remove_mixin_test_remove_builder(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_REMOVE_BUILDER_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', WATERFALL_MIXIN_REMOVE_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension_sets_application(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_DIMENSION_SETS_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_NO_DIMENSIONS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_DIMENSION_SETS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', WATERFALL_DIMENSION_SETS_WATERFALL_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', WATERFALL_DIMENSION_SETS_WATERFALL_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

TEST_SUITE_WITH_PARAMS = """\
{
  'basic_suites': {
    'bar_tests': {
      'bar_test': {
        'args': ['--no-xvfb'],
        'swarming': {
          'dimension_sets': [
            {
              'device_os': 'NMF26U'
            }
          ],
        },
        'should_retry_with_patch': False,
        'name': 'bar_test'
      },
      'bar_test_test': {
        'swarming': {
          'dimension_sets': [
            {
              'kvm': '1'
            }
          ],
          'hard_timeout': 1000
        },
        'should_retry_with_patch': True
      }
    },
    'foo_tests': {
      'foo_test_empty': {},
      'foo_test': {
        'args': [
          '--jobs=1',
          '--verbose'
        ],
        'swarming': {
          'dimension_sets': [
            {
              'device_os': 'MMB29Q'
            }
          ],
          'hard_timeout': 1800
        }
      },
      'foo_test_test': {
        'swarming': {
        },
        'name': 'pls'
      },
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
TEST_QUERY_BOTS_OUTPUT = {
  "Fake Android M Tester": {
    "gtest_tests": [
      {
        "test": "foo_test",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": False
        }
      }
    ]
  },
  "Fake Android L Tester": {
    "gtest_tests": [
      {
        "test": "foo_test",
        "args": [
          "--gs-results-bucket=chromium-result-details",
          "--recover-devices"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision":
              "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets":[
            {
              "device_os": "LMY41U",
              "device_os_type": "user",
              "device_type": "hammerhead",
              'os': 'Android'
            }
          ],
          "can_use_on_swarming_builders": True
        }
      }
    ]
  },
  "Fake Android K Tester": {
    "additional_compile_targets": ["bar_test"],
    "gtest_tests": [
      {
        "test": "foo_test",
        "args": [
          "--gs-results-bucket=chromium-result-details",
          "--recover-devices"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "cipd_packages": [
            {
              "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
              "location": "bin",
              "revision":
              "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
            }
          ],
          "dimension_sets": [
            {
              "device_os": "KTU84P",
              "device_os_type": "userdebug",
              "device_type": "hammerhead",
              "os": "Android",
            }
          ],
          "can_use_on_swarming_builders": True,
          "output_links": [
            {
              "link": ["https://luci-logdog.appspot.com/v/?s",
              "=android%2Fswarming%2Flogcats%2F",
              "${TASK_ID}%2F%2B%2Funified_logcats"],
              "name": "shard #${SHARD_INDEX} logcats"
            }
          ]
        }
      }
    ]
  },
  "Android Builder": {
    "additional_compile_targets": ["bar_test"]
  }
}
TEST_QUERY_BOTS_TESTS_OUTPUT = {
  "Fake Android M Tester": [
    {
      "merge": {
        "args": [],
        "script": "//testing/merge_scripts/standard_gtest_merge.py"
      },
      "test": "foo_test",
      "swarming": {
        "can_use_on_swarming_builders": False
      }
    }
  ],
  "Fake Android L Tester": [
    {
      "test": "foo_test",
      "args": [
        "--gs-results-bucket=chromium-result-details",
        "--recover-devices"
      ],
      "merge": {
        "args": [],
        "script": "//testing/merge_scripts/standard_gtest_merge.py"
      },
      "swarming": {
        "cipd_packages": [
          {
            "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
            "location": "bin",
            "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
          }
        ],
        "dimension_sets": [
          {
            "device_os": "LMY41U",
            "device_os_type": "user",
            "device_type": "hammerhead",
            "os": "Android"
          }
        ],
        "can_use_on_swarming_builders": True
      }
    }
  ],
  "Android Builder": [],
  "Fake Android K Tester": [
    {
      "test": "foo_test",
      "args": [
        "--gs-results-bucket=chromium-result-details",
        "--recover-devices"
      ],
      "merge": {
        "args": [],
        "script": "//testing/merge_scripts/standard_gtest_merge.py"
      },
      "swarming": {
        "cipd_packages": [
          {
            "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
            "location": "bin",
            "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
          }
        ],
        "dimension_sets": [
          {
            "device_os": "KTU84P",
            "device_os_type": "userdebug",
            "device_type": "hammerhead",
            "os": "Android"
          }
        ],
        "can_use_on_swarming_builders": True,
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
      }
    }
  ]
}

TEST_QUERY_BOT_OUTPUT = {
  "additional_compile_targets": ["bar_test"],
  "gtest_tests": [
    {
      "test": "foo_test",
      "args": [
        "--gs-results-bucket=chromium-result-details",
        "--recover-devices"
      ],
      "merge": {
        "args": [],
        "script": "//testing/merge_scripts/standard_gtest_merge.py"
      },
      "swarming": {
        "cipd_packages": [
          {
            "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
            "location": "bin",
            "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
          }
        ],
        "dimension_sets": [
          {
            "device_os": "KTU84P",
            "device_os_type": "userdebug",
            "device_type": "hammerhead",
            "os": "Android"
          }
        ],
        "can_use_on_swarming_builders": True,
        "output_links": [
          {
            "link": ["https://luci-logdog.appspot.com/v/?s",
            "=android%2Fswarming%2Flogcats%2F",
            "${TASK_ID}%2F%2B%2Funified_logcats"
          ],
          "name": "shard #${SHARD_INDEX} logcats"
          }
        ]
      }
    }
  ]
}
TEST_QUERY_BOT_TESTS_OUTPUT = [
  {
    "test": "foo_test",
    "args": [
      "--gs-results-bucket=chromium-result-details",
      "--recover-devices"
    ],
    "merge": {
      "args": [],
      "script": "//testing/merge_scripts/standard_gtest_merge.py"
    },
    "swarming": {
      "cipd_packages": [
        {
          "cipd_package": "infra/tools/luci/logdog/butler/${platform}",
          "location": "bin",
          "revision": "git_revision:ff387eadf445b24c935f1cf7d6ddd279f8a6b04c"
        }
      ],
      "dimension_sets": [
        {
          "device_os": "LMY41U",
          "device_os_type": "user",
          "device_type": "hammerhead",
          "os": "Android"
        }
      ],
      "can_use_on_swarming_builders": True
    }
  }
]

TEST_QUERY_TESTS_OUTPUT = {
  "bar_test": {},
  "foo_test": {}
}

TEST_QUERY_TESTS_MULTIPLE_PARAMS_OUTPUT = ["foo_test"]

TEST_QUERY_TESTS_DIMENSION_PARAMS_OUTPUT = ["bar_test"]

TEST_QUERY_TESTS_SWARMING_PARAMS_OUTPUT = ["bar_test_test"]

TEST_QUERY_TESTS_PARAMS_OUTPUT = ['bar_test_test']

TEST_QUERY_TESTS_PARAMS_FALSE_OUTPUT = ['bar_test']

TEST_QUERY_TEST_OUTPUT = {}

TEST_QUERY_TEST_BOTS_OUTPUT = [
    "Fake Android K Tester",
    "Fake Android L Tester",
    "Fake Android M Tester",
]

TEST_QUERY_TEST_BOTS_ISOLATED_SCRIPTS_OUTPUT = ['Fake Tester']

TEST_QUERY_TEST_BOTS_NO_BOTS_OUTPUT = []


class QueryTests(TestCase):
  """Tests for the query feature."""
  def test_query_bots(self):
    self.override_args(query='bots',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOTS_OUTPUT)

  def test_query_bots_invalid(self):
    self.override_args(query='bots/blah/blah',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_bots_json(self):
    self.override_args(query='bots',
                       check=False,
                       pyl_files_dir=None,
                       json='result.json',
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    self.assertFalse(fbb.printed_lines)

  def test_query_bots_tests(self):
    self.override_args(query='bots/tests',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOTS_TESTS_OUTPUT)

  def test_query_invalid_bots_tests(self):
    self.override_args(query='bots/tdfjdk',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_bot(self):
    self.override_args(query='bot/Fake Android K Tester',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.maxDiff = None  # pragma pylint: disable=attribute-defined-outside-init
    self.assertEqual(query_json, TEST_QUERY_BOT_OUTPUT)

  def test_query_bot_invalid_id(self):
    self.override_args(query='bot/bot1',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_bot_invalid_query_too_many(self):
    self.override_args(query='bot/Fake Android K Tester/blah/blah',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_bot_invalid_query_no_tests(self):
    self.override_args(query='bot/Fake Android K Tester/blahs',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_bot_tests(self):
    self.override_args(query='bot/Fake Android L Tester/tests',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOT_TESTS_OUTPUT)

  def test_query_tests(self):
    self.override_args(query='tests',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_OUTPUT)

  def test_query_tests_invalid(self):
    self.override_args(query='tests/blah/blah',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_tests_multiple_params(self):
    self.override_args(query='tests/--jobs=1&--verbose',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_MULTIPLE_PARAMS_OUTPUT)

  def test_query_tests_invalid_params(self):
    self.override_args(query='tests/device_os?',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_tests_dimension_params(self):
    self.override_args(query='tests/device_os:NMF26U',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_DIMENSION_PARAMS_OUTPUT)

  def test_query_tests_swarming_params(self):
    self.override_args(query='tests/hard_timeout:1000',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_SWARMING_PARAMS_OUTPUT)

  def test_query_tests_params(self):
    self.override_args(query='tests/should_retry_with_patch:true',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_PARAMS_OUTPUT)

  def test_query_tests_params_false(self):
    self.override_args(query='tests/should_retry_with_patch:false',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_PARAMS_FALSE_OUTPUT)

  def test_query_test(self):
    self.override_args(query='test/foo_test',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_OUTPUT)

  def test_query_test_invalid_id(self):
    self.override_args(query='test/foo_foo',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_test_invalid_length(self):
    self.override_args(query='test/foo_tests/foo/foo',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_test_bots(self):
    self.override_args(query='test/foo_test/bots',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_OUTPUT)

  def test_query_test_bots_isolated_scripts(self):
    self.override_args(query='test/foo_test/bots',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_ISOLATED_SCRIPTS_OUTPUT)

  def test_query_test_bots_invalid(self):
    self.override_args(query='test/foo_tests/foo',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)

  def test_query_test_bots_no_bots(self):
    self.override_args(query='test/bar_tests/bots',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads("".join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_NO_BOTS_OUTPUT)

  def test_query_invalid(self):
    self.override_args(query='foo',
                       check=False,
                       pyl_files_dir=None,
                       json=None,
                       waterfall_filters=[])
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    with self.assertRaises(SystemExit) as cm:
      fbb.query(fbb.args)
      self.assertEqual(cm.exception.code, 1)
    self.assertTrue(fbb.printed_lines)


FOO_TEST_SUITE_WITH_ENABLE_FEATURES_SEPARATE_ENTRIES = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'args': [
          '--enable-features',
          'Foo,Bar',
        ],
      },
    },
  },
}
"""


FOO_TEST_REPLACEMENTS_REMOVE_NO_VALUE = """\
{
  'foo_test': {
    'replacements': {
      'Fake Tester': {
        'args': {
          '--c_arg': None,
        },
      },
    },
  },
}
"""


FOO_TEST_REPLACEMENTS_REMOVE_VALUE = """\
{
  'foo_test': {
    'replacements': {
      'Fake Tester': {
        'args': {
          '--enable-features': None,
        },
      },
    },
  },
}
"""


FOO_TEST_REPLACEMENTS_REPLACE_VALUE = """\
{
  'foo_test': {
    'replacements': {
      'Fake Tester': {
        'args': {
          '--enable-features': 'Bar,Baz',
        },
      },
    },
  },
}
"""


FOO_TEST_REPLACEMENTS_INVALID_KEY = """\
{
  'foo_test': {
    'replacements': {
      'Fake Tester': {
        'invalid': {
          '--enable-features': 'Bar,Baz',
        },
      },
    },
  },
}
"""


REPLACEMENTS_REMOVE_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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

REPLACEMENTS_VALUE_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--enable-features=Bar,Baz"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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

REPLACEMENTS_VALUE_SEPARATE_ENTRIES_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--enable-features",
          "Bar,Baz"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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


class ReplacementTests(TestCase):
  """Tests for the arg replacement feature."""
  def test_replacement_valid_remove_no_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REMOVE_NO_VALUE)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           REPLACEMENTS_REMOVE_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           REPLACEMENTS_REMOVE_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_remove_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REMOVE_VALUE)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           REPLACEMENTS_REMOVE_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           REPLACEMENTS_REMOVE_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_replace_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REPLACE_VALUE)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           REPLACEMENTS_VALUE_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           REPLACEMENTS_VALUE_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_replace_value_separate_entries(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES_SEPARATE_ENTRIES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REPLACE_VALUE)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', REPLACEMENTS_VALUE_SEPARATE_ENTRIES_OUTPUT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', REPLACEMENTS_VALUE_SEPARATE_ENTRIES_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_invalid_key_not_valid(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_INVALID_KEY)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Given replacement key *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_replacement_invalid_key_not_found(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REPLACE_VALUE)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Could not find *'):
      fbb.check_output_file_consistency(verbose=True)


FOO_TEST_SUITE_WITH_MAGIC_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'args': [
          '$$MAGIC_SUBSTITUTION_TestOnlySubstitution',
        ],
      },
    },
  },
}
"""


FOO_TEST_SUITE_WITH_INVALID_MAGIC_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'args': [
          '$$MAGIC_SUBSTITUTION_NotARealSubstitution',
        ],
      },
    },
  },
}
"""


MAGIC_SUBSTITUTIONS_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--magic-substitution-success"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
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


class MagicSubstitutionTests(TestCase):
  """Tests for the magic substitution feature."""
  def test_valid_function(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MAGIC_ARGS, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MAGIC_SUBSTITUTIONS_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           MAGIC_SUBSTITUTIONS_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_invalid_function(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_INVALID_MAGIC_ARGS, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Magic substitution function *'):
      fbb.check_output_file_consistency(verbose=True)


# Matrix compound composition test suites

MATRIX_COMPOUND_EMPTY = """\
{
  'basic_suites': {
    'bar_tests': {
      'bar_test': {},
    },
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {},
      'bar_tests': {},
    },
  },
}
"""

MATRIX_COMPOUND_MISSING_IDENTIFIER = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'swarming': {
              'dimension_sets': [
                {
                  'foo': 'bar',
                },
              ],
            },
          },
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_EMPTY_IDENTIFIER = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'identifier': '',
            'swarming': {
              'dimension_sets': [
                {
                  'foo': 'empty identifier not allowed',
                },
              ],
            },
          },
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_TRAILING_IDENTIFIER = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'identifier': ' ',
            'swarming': {
              'dimension_sets': [
                {
                  'foo': 'strip to empty not allowed',
                },
              ],
            },
          },
        ],
      },
      'foo_tests': {
        'variants': [
          {
            'identifier': 'id ',
            'swarming': {
              'dimension_sets': [
                {
                  'foo': 'trailing whitespace not allowed',
                },
              ],
            },
          },
        ],
      },
    },
  },
}
"""

MATRIX_MISMATCHED_SWARMING_LENGTH = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'dimension_sets': [
            {
              'hello': 'world',
            }
          ],
        },
      },
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'identifier': 'test',
            'swarming': {
              'dimension_sets': [
                {
                  'foo': 'bar',
                },
                {
                  'bar': 'foo',
                }
              ],
            },
          },
        ],
      },
    },
  },
}
"""

MATRIX_REF_NONEXISTENT = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'bar_test': {},
    },
  },
}
"""

MATRIX_COMPOUND_REF_COMPOSITION = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'compound_suites': {
    'sample_composition': {
      'foo_tests': {},
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'sample_composition': {},
    },
  },
}
"""

MATRIX_COMPOSITION_REF_MATRIX = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {},
    },
  },
  'matrix_compound_suites': {
    'a_test': {
      'foo_tests': {},
    },
    'matrix_tests': {
      'a_test': {},
    },
  },
}
"""

MATRIX_COMPOUND_VARIANTS_MIXINS_MERGE = """\
{
  'basic_suites': {
    'foo_tests': {
      'set': {
        'mixins': [ 'test_mixin' ],
      },
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'mixins': [ 'dimension_mixin' ],
          },
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_VARIANTS_MIXINS = """\
{
  'basic_suites': {
    'foo_tests': {
      'set': {
        'mixins': [ 'test_mixin' ],
      },
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'mixins': [
              'dimension_mixin',
              'waterfall_mixin',
              'builder_mixin',
              'random_mixin'
            ],
          },
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_VARIANTS_MIXINS_REMOVE = """\
{
  'basic_suites': {
    'foo_tests': {
      'set': {
        'remove_mixins': ['builder_mixin'],
      },
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'mixins': [ 'builder_mixin' ],
          }
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_CONFLICTING_TEST_SUITES = """\
{
  'basic_suites': {
    'bar_tests': {
      'baz_tests': {
        'args': [
          '--bar',
        ],
      }
    },
    'foo_tests': {
      'baz_tests': {
        'args': [
          '--foo',
        ],
      }
    },
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'bar_tests': {
        'variants': [
          {
            'identifier': 'bar',
          }
        ],
      },
      'foo_tests': {
        'variants': [
          {
            'identifier': 'foo'
          }
        ]
      }
    },
  },
}
"""

MATRIX_COMPOUND_TARGETS_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'args_test': {
        'args': [
          '--iam'
        ],
      },
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'identifier': 'args',
            'args': [
              '--anarg',
            ],
          },
          {
            'identifier': 'swarming',
            'swarming': {
              'a': 'b',
              'dimension_sets': [
                {
                  'hello': 'world',
                }
              ]
            }
          },
          {
            'identifier': 'mixins',
            'mixins': [ 'dimension_mixin' ],
          }
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_TARGETS_MIXINS = """\
{
  'basic_suites': {
    'foo_tests': {
      'mixins_test': {
        'mixins': [ 'test_mixin' ],
      },
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'mixins': [ 'random_mixin' ],
        'variants': [
          {
            'identifier': 'args',
            'args': [
              '--anarg',
            ],
          },
          {
            'identifier': 'swarming',
            'swarming': {
              'a': 'b',
              'dimension_sets': [
                {
                  'hello': 'world',
                }
              ]
            }
          },
          {
            'identifier': 'mixins',
            'mixins': [ 'dimension_mixin' ],
          }
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_TARGETS_SWARMING = """\
{
  'basic_suites': {
    'foo_tests': {
      'swarming_test': {
        'swarming': {
          'foo': 'bar',
          'dimension_sets': [
            {
              'foo': 'bar',
            },
          ],
        },
      },
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          {
            'identifier': 'args',
            'args': [
              '--anarg',
            ],
          },
          {
            'identifier': 'swarming',
            'swarming': {
              'a': 'b',
              'dimension_sets': [
                {
                  'hello': 'world',
                }
              ]
            }
          },
          {
            'identifier': 'mixins',
            'mixins': [ 'dimension_mixin' ],
          }
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_VARIANTS_REF = """\
{
  'basic_suites': {
    'foo_tests': {
      'swarming_test': {},
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          'a_variant'
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_TEST_WITH_TEST_KEY = """\
{
  'basic_suites': {
    'foo_tests': {
      'swarming_test': {
          'test': 'foo_test_apk'
      },
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          'a_variant',
        ],
      },
    },
  },
}
"""

MATRIX_COMPOUND_MIXED_VARIANTS_REF = """\
{
  'basic_suites': {
    'foo_tests': {
      'swarming_test': {},
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          'a_variant',
          {
            'args': [
              'a',
              'b'
            ],
            'identifier': 'ab',
          }
        ],
      },
    },
  },
}
"""

VARIANTS_FILE = """\
{
  'a_variant': {
    'args': [
      '--platform',
      'device',
      '--version',
      '1'
    ],
    'identifier': 'a_variant'
  }
}
"""

MULTI_VARIANTS_FILE = """\
{
  'a_variant': {
    'args': [
      '--platform',
      'device',
      '--version',
      '1'
    ],
    'identifier': 'a_variant'
  },
  'b_variant': {
    'args': [
      '--platform',
      'sim',
      '--version',
      '2'
    ],
    'identifier': 'b_variant'
  }
}
"""

# # Dictionary composition test suite outputs

MATRIX_COMPOUND_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "bar_test"
      },
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MATRIX_COMPOUND_TEST_SUITE_WITH_TEST_KEY_DICT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--platform",
          "device",
          "--version",
          "1"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "swarming_test a_variant",
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test_apk"
      }
    ]
  }
}
"""

MATRIX_TARGET_DICT_MERGE_OUTPUT_ARGS = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--iam",
          "--anarg"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "args_test args",
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "args_test"
      },
      {
        "args": [
          "--iam"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "args_test mixins",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "iama": "mixin"
            }
          ]
        },
        "test": "args_test"
      },
      {
        "args": [
          "--iam"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "args_test swarming",
        "swarming": {
          "a": "b",
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "hello": "world"
            }
          ]
        },
        "test": "args_test"
      }
    ]
  }
}
"""

MATRIX_TARGET_DICT_MERGE_OUTPUT_MIXINS = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--anarg"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "mixins_test args",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "value": "test"
        },
        "test": "mixins_test",
        "value": "random"
      },
      {
        "args": [],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "mixins_test mixins",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "iama": "mixin"
            }
          ],
          "value": "test"
        },
        "test": "mixins_test",
        "value": "random"
      },
      {
        "args": [],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "mixins_test swarming",
        "swarming": {
          "a": "b",
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "hello": "world"
            }
          ],
          "value": "test"
        },
        "test": "mixins_test",
        "value": "random"
      }
    ]
  }
}
"""

MATRIX_TARGET_DICT_MERGE_OUTPUT_SWARMING = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--anarg"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "swarming_test args",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "foo": "bar"
            }
          ],
          "foo": "bar"
        },
        "test": "swarming_test"
      },
      {
        "args": [],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "swarming_test mixins",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "foo": "bar",
              "iama": "mixin"
            }
          ],
          "foo": "bar"
        },
        "test": "swarming_test"
      },
      {
        "args": [],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "swarming_test swarming",
        "swarming": {
          "a": "b",
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "foo": "bar",
              "hello": "world"
            }
          ],
          "foo": "bar"
        },
        "test": "swarming_test"
      }
    ]
  }
}
"""

MATRIX_COMPOUND_VARIANTS_REF_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "gtest_tests": [
      {
        "args": [
          "--platform",
          "device",
          "--version",
          "1"
        ],
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "name": "swarming_test a_variant",
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "swarming_test"
      }
    ]
  }
}
"""

EMPTY_SKYLAB_TEST_EXCEPTIONS = """\
{
  'tast.foo OCTOPUS_TOT': {
    'remove_from': [
      'Fake Tester',
    ]
  },
  'tast.foo OCTOPUS_TOT-1': {
    'remove_from': [
      'Fake Tester',
    ]
  }
}
"""

MATRIX_SKYLAB_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'test_suites': {
          'skylab_tests': 'cros_skylab_basic_x86',
        },
      },
    },
  },
]
"""

MATRIX_COMPOUND_SKYLAB_REF = """\
{
  'basic_suites': {
    'cros_skylab_basic': {
      'tast.basic': {
        'suite': 'tast.basic',
        'timeout': 3600,
      },
      'tast.foo': {
        'suite': 'tast.foo',
        'timeout': 3600,
      },
    },
  },
  'compound_suites': {},
  'matrix_compound_suites': {
    'cros_skylab_basic_x86': {
      'cros_skylab_basic': {
        'variants': [
          {
            'skylab': {
              'cros_board': 'octopus',
              'cros_chrome_version': '89.0.3234.0',
              'cros_img': 'octopus-release/R89-13655.0.0',
            },
            'enabled': True,
            'identifier': 'OCTOPUS_TOT',
          },
          {
            'skylab': {
              'cros_board': 'octopus',
              'cros_chrome_version': '88.0.2324.0',
              'cros_img': 'octopus-release/R88-13597.23.0',
            },
            'enabled': True,
            'identifier': 'OCTOPUS_TOT-1',
          },
        ]
      },
    },
  },
}
"""

ENABLED_AND_DISABLED_MATRIX_COMPOUND_SKYLAB_REF = """\
{
  'basic_suites': {
    'cros_skylab_basic': {
      'tast.basic': {
        'suite': 'tast.basic',
        'timeout': 3600,
      },
      'tast.foo': {
        'suite': 'tast.foo',
        'timeout': 3600,
      },
    },
  },
  'compound_suites': {},
  'matrix_compound_suites': {
    'cros_skylab_basic_x86': {
      'cros_skylab_basic': {
        'variants': [
          {
            'skylab': {
              'cros_board': 'octopus',
              'cros_chrome_version': '89.0.3234.0',
              'cros_img': 'octopus-release/R89-13655.0.0',
            },
            'enabled': True,
            'identifier': 'OCTOPUS_TOT',
          },
          {
            'skylab': {
              'cros_board': 'octopus',
              'cros_chrome_version': '88.0.2324.0',
              'cros_img': 'octopus-release/R88-13597.23.0',
            },
            'enabled': False,
            'identifier': 'OCTOPUS_TOT-1',
          },
        ]
      },
    },
  },
}
"""

VARIATION_SKYLAB_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "skylab_tests": [
      {
        "args": [],
        "cros_board": "octopus",
        "cros_img": "octopus-release/R89-13655.0.0",
        "name": "tast.basic OCTOPUS_TOT",
        "suite": "tast.basic",
        "swarming": {},
        "test": "tast.basic",
        "timeout": 3600
      },
      {
        "args": [],
        "cros_board": "octopus",
        "cros_img": "octopus-release/R88-13597.23.0",
        "name": "tast.basic OCTOPUS_TOT-1",
        "suite": "tast.basic",
        "swarming": {},
        "test": "tast.basic",
        "timeout": 3600
      }
    ]
  }
}
"""

ENABLED_AND_DISABLED_VARIATION_SKYLAB_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Fake Tester": {
    "skylab_tests": [
      {
        "args": [],
        "cros_board": "octopus",
        "cros_img": "octopus-release/R89-13655.0.0",
        "name": "tast.basic OCTOPUS_TOT",
        "suite": "tast.basic",
        "swarming": {},
        "test": "tast.basic",
        "timeout": 3600
      }
    ]
  }
}
"""


class MatrixCompositionTests(TestCase):

  def test_good_structure_no_configs(self):
    """
    Tests matrix compound test suite structure with no configs,
    no conflicts and no bad references
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_EMPTY, LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MATRIX_COMPOUND_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           MATRIX_COMPOUND_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_missing_identifier(self):
    """
    Variant is missing an identifier
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_MISSING_IDENTIFIER, LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Missing required identifier field in matrix compound suite*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_empty_identifier(self):
    """
    Variant identifier is empty.
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_EMPTY_IDENTIFIER, LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Identifier field can not be "" in matrix compound suite*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_trailing_identifier(self):
    """
    Variant identifier has trailing whitespace.
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TRAILING_IDENTIFIER, LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Identifier field can not have leading and trailing whitespace in'
        ' matrix compound suite*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_mismatched_swarming_length(self):
    """
    Swarming dimension set length mismatch test. Composition set > basic set
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_MISMATCHED_SWARMING_LENGTH, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Error merging lists by key *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_noexistent_ref(self):
    """
    Test referencing a non-existent basic test suite
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_REF_NONEXISTENT, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Unable to find reference to *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_ref_to_composition(self):
    """
    Test referencing another composition test suite
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_REF_COMPOSITION, LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'matrix_compound_suites may not refer to other *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_ref_to_matrix(self):
    """
    Test referencing another matrix test suite
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOSITION_REF_MATRIX, LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'matrix_compound_suites may not refer to other *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_conflicting_names(self):
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_CONFLICTING_TEST_SUITES, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Conflicting test definitions.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_swarming_dict_merge_args(self):
    """
    Test targets with swarming dictionary defined by both basic and matrix
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TARGETS_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_ARGS)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_ARGS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_swarming_dict_merge_mixins(self):
    """
    Test targets with swarming dictionary defined by both basic and matrix
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TARGETS_MIXINS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_swarming_dict_swarming(self):
    """
    Test targets with swarming dictionary defined by both basic and matrix
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TARGETS_SWARMING,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_SWARMING)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', MATRIX_TARGET_DICT_MERGE_OUTPUT_SWARMING)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variant_test_suite_with_test_key(self):
    """
    Test targets in matrix compound test suites with variants
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TEST_WITH_TEST_KEY,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', MATRIX_COMPOUND_TEST_SUITE_WITH_TEST_KEY_DICT)
    self.create_testing_buildbot_json_file(
        'chromium.ci.json', MATRIX_COMPOUND_TEST_SUITE_WITH_TEST_KEY_DICT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_pyl_ref(self):
    """Test targets with variants string ref"""
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MATRIX_COMPOUND_VARIANTS_REF_OUTPUT)
    self.create_testing_buildbot_json_file('chromium.ci.json',
                                           MATRIX_COMPOUND_VARIANTS_REF_OUTPUT)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_pyl_no_ref(self):
    """Test targets with variants string ref, not defined in variants.pyl"""
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_VARIANTS_REF, LUCI_MILO_CFG)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Missing variant definition for *'):
      fbb.check_output_file_consistency(verbose=True)

  def test_variants_pyl_all_unreferenced(self):
    """Test targets with variants in variants.pyl, unreferenced in tests"""
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_MIXED_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    variants=MULTI_VARIANTS_FILE)
    # self.create_testing_buildbot_json_file(
    #     'chromium.test.json', MATRIX_COMPOUND_VARIANTS_REF_OUTPUT)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'The following variants were unreferenced *'):
      fbb.check_input_file_consistency(verbose=True)

  def test_good_skylab_matrix_with_variants(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL,
                    MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           VARIATION_SKYLAB_OUTPUT)
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_enabled_and_disabled_skylab_matrix_with_variants(self):
    """Test with disabled variants"""
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL,
                    ENABLED_AND_DISABLED_MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS)
    # some skylab test variant is disabled; the corresponding skylab tests
    # is not generated.
    self.create_testing_buildbot_json_file(
        'chromium.test.json', ENABLED_AND_DISABLED_VARIATION_SKYLAB_OUTPUT)
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)


MAC_TEST_SUITE = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
      },
    },
  },
}
"""

MAC_GTESTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Mac': {
        'swarming': {
          'can_use_on_swarming_builders': True,
        },
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

MAC_GTEST_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Mac": {
    "gtest_tests": [
      {
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_gtest_merge.py"
        },
        "swarming": {
          "can_use_on_swarming_builders": true
        },
        "test": "foo_test"
      }
    ]
  }
}
"""

MAC_ISOLATED_SCRIPTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Mac': {
        'swarming': {
          'dimension_sets': [
            {
              'os': 'Mac',
            },
          ],
        },
        'test_suites': {
          'isolated_scripts': 'foo_tests',
        },
      },
    },
  },
]
"""

MAC_ISOLATED_SCRIPTS_WATERFALL_OUTPUT = """\
{
  "AAAAA1 AUTOGENERATED FILE DO NOT EDIT": {},
  "AAAAA2 See generate_buildbot_json.py to make changes": {},
  "Mac": {
    "isolated_scripts": [
      {
        "isolate_name": "foo_test",
        "merge": {
          "args": [],
          "script": "//testing/merge_scripts/standard_isolated_script_merge.py"
        },
        "name": "foo_test",
        "swarming": {
          "can_use_on_swarming_builders": true,
          "dimension_sets": [
            {
              "os": "Mac"
            }
          ]
        }
      }
    ]
  }
}
"""

MAC_LUCI_MILO_CFG = """\
consoles {
  builders {
    name: "buildbucket/luci.chromium.ci/Mac"
  }
}
"""


class SwarmingTests(TestCase):
  def test_mac_builder_with_no_cpu_dimension_in_gtest_fails(self):
    fbb = FakeBBGen(self.args, MAC_GTESTS_WATERFALL, MAC_TEST_SUITE,
                    MAC_LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file('chromium.test.json',
                                           MAC_GTEST_WATERFALL_OUTPUT)
    fbb.check_input_file_consistency(verbose=True)
    self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                           'os and cpu',
                           fbb.check_output_file_consistency,
                           verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mac_builder_with_no_cpu_dimension_in_isolated_script_fails(self):
    fbb = FakeBBGen(self.args, MAC_ISOLATED_SCRIPTS_WATERFALL, MAC_TEST_SUITE,
                    MAC_LUCI_MILO_CFG)
    self.create_testing_buildbot_json_file(
        'chromium.test.json', MAC_ISOLATED_SCRIPTS_WATERFALL_OUTPUT)
    fbb.check_input_file_consistency(verbose=True)
    self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                           'os and cpu',
                           fbb.check_output_file_consistency,
                           verbose=True)
    self.assertFalse(fbb.printed_lines)


if __name__ == '__main__':
  unittest.main()

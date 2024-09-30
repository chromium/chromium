#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_buildbot_json.py."""

import contextlib
import json
import os
import re
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

    expectations_dir = os.path.join(THIS_DIR, 'unittest_expectations')
    self.output_dir = os.path.join(expectations_dir, self.id().split('.')[-1])

    # This allows reads into this directory to fallback to the real fs, which we
    # want so each test case's json can be checked-in.
    self.fs.add_real_directory(expectations_dir)

    self.set_args()

  def set_args(self, *args):
    self.args = generate_buildbot_json.BBJSONGenerator.parse_args((
        '--output-dir',
        self.output_dir,
    ) + args)

  def regen_test_json(self, fakebb):
    """Regenerates a unittest's json files.

    Useful when making sweeping changes to pyl inputs in many test cases. Can be
    used by simply inserting a call to this method in a unit test after
    initializing its FakeBBGen object.
    """
    contents = fakebb.generate_outputs()
    with fake_filesystem_unittest.Pause(self.fs):
      try:
        os.mkdir(self.output_dir)
      except FileExistsError:
        if not os.path.isdir(self.output_dir):
          raise

      fakebb.write_json_result(contents)


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
               autoshard_exceptions=json.dumps({}),
               mixins=EMPTY_PYL_FILE,
               gn_isolate_map=EMPTY_PYL_FILE,
               variants=EMPTY_PYL_FILE):
    super(FakeBBGen, self).__init__(args)

    pyl_files_dir = args.pyl_files_dir or THIS_DIR
    infra_config_dir = args.infra_config_dir

    files = {
        args.waterfalls_pyl_path: waterfalls,
        args.test_suites_pyl_path: test_suites,
        args.test_suite_exceptions_pyl_path: exceptions,
        args.autoshard_exceptions_json_path: autoshard_exceptions,
        args.mixins_pyl_path: mixins,
        args.gn_isolate_map_pyl_path: gn_isolate_map,
        args.variants_pyl_path: variants,
        os.path.join(pyl_files_dir, 'gn_isolate_map2.pyl'):
        GPU_TELEMETRY_GN_ISOLATE_MAP,
        os.path.join(infra_config_dir, 'generated/project.pyl'): project_pyl,
        os.path.join(infra_config_dir, 'generated/luci/luci-milo.cfg'):
        luci_milo_cfg,
        os.path.join(infra_config_dir, 'generated/luci/luci-milo-dev.cfg'): '',
    }
    for path, content in files.items():
      if content is None:
        continue
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
          'dimensions':{
            'kvm': '1',
            'os': 'Linux',
          },
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
          'dimensions': {
            "device_type": "foo_device",
          },
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
          'dimensions': {
            'gpu': '10de:1cb3',
          },
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
          'dimensions': {
            'device_type': 'bullhead',
          },
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
          'dimensions': {
            'device_type': 'bullhead',
          },
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
          'dimensions': {
            'kvm': '1',
          },
        },
        'test_suites': {
          'gpu_telemetry_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_GPU_TELEMETRY_TEST_WATERFALL_CAST_STREAMING = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'fuchsia',
        'browser_config': 'not-a-real-browser',
        'swarming': {
          'dimensions': {
            'kvm': '1',
          },
        },
        'test_suites': {
          'cast_streaming_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

FOO_GPU_TELEMETRY_TEST_WATERFALL_SKYLAB = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Fake Tester': {
        'os_type': 'chromeos',
        'browser_config': 'cros-chrome',
        'use_swarming': False,
        'swarming': {
          'some_key': 'some_value',
        },
        'test_suites': {
          'skylab_gpu_telemetry_tests': 'composition_tests',
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
          'dimensions': {
            'gpu': '10de:1cb3-26.21.14.3102',
          },
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
          'dimensions': {
            'gpu': '8086:5912-24.20.100.6286',
          },
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
          'dimensions': {
            'gpu': '8086:3e92-24.20.100.6286',
          },
        },
        'test_suites': {
          'gpu_telemetry_tests': 'composition_tests',
        },
      },
    },
  },
]
"""

GPU_TELEMETRY_TEST_VARIANTS_WATERFALL = """\
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
          'dimensions': {
            'gpu': '8086:3e92-24.20.100.6286',
            'os': 'Linux',
          },
        },
        'test_suites': {
          'gpu_telemetry_tests': 'matrix_tests',
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
          'dimensions': {
            'device_os': 'KTU84P',
            'device_type': 'hammerhead',
            'os': 'Android',
          },
        },
        'os_type': 'android',
        'skip_merge_script': True,
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
      'Fake Android L Tester': {
        'swarming': {
          'dimensions': {
            'device_os': 'LMY41U',
            'device_os_type': 'user',
            'device_type': 'hammerhead',
            'os': 'Android',
          },
        },
        'os_type': 'android',
        'skip_merge_script': True,
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
      'Fake Android M Tester': {
        'swarming': {
          'dimensions': {
            'device_os': 'MMB29Q',
            'device_type': 'bullhead',
            'os': 'Android',
          },
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
          'dimensions': {
            'integrity': 'high',
            'os': 'Linux',
          },
          'expiration': 120,
        },
      },
    },
  },
}
"""

FOO_TEST_SUITE_ANDROID_SWARMING = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'android_swarming': {
          'shards': 100,
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

FOO_TEST_SUITE_WITH_SWARMING_DIMENSION_SETS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'dimension_sets': [
            {
              'foo': 'bar',
            },
          ],
        },
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_NAME = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'name': 'bar_test',
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_ISOLATE_NAME = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'isolate_name': 'bar_test',
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
          'dimensions': {
            'integrity': 'high',
          },
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
          'dimensions': {
            'integrity': 'high',
          },
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
        'args': ['common-arg'],
        'precommit_args': ['precommit-arg'],
        'non_precommit_args': ['non-precommit-arg'],
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
      'bar_test': {
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
      },
    },
    'foo_tests': {
      'foo_test': {
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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

COMPOSITION_SUITE_WITH_TELEMETRY_TEST_WITH_INVALID_NAME = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo': {
        'telemetry_test_name': 'foo',
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
      },
    },
  },
  'compound_suites': {
    'composition_tests': [
      'foo_tests',
    ],
  },
}
"""

COMPOSITION_SUITE_WITH_TELEMETRY_TEST = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_tests': {
        'telemetry_test_name': 'foo',
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
          'shards': 2,
        },
      },
    },
    'bar_tests': {
      'bar_test': {
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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

COMPOSITION_SUITE_WITH_GPU_ARGS = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_tests': {
        'telemetry_test_name': 'foo',
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
        'args': [
          '--gpu-vendor-id',
          '${gpu_vendor_id}',
          '--gpu-device-id',
          '${gpu_device_id}',
        ],
      },
    },
    'bar_tests': {
      'bar_test': {
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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

AUTOSHARD_EXCEPTIONS = json.dumps({
    'chromium.test': {
        'Fake Tester': {
            'foo_test': {
                'shards': 20,
            },
            'foo_test_apk': {
                'shards': 2,
            },
            'swarming_test a_variant': {
                'shards': 10,
            },
        }
    },
})

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
          'dimensions': {
            'integrity': None,
          },
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

GPU_TELEMETRY_GN_ISOLATE_MAP_CAST_STREAMING = """\
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

  def test_dimension_sets_causes_error(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_SWARMING_DIMENSION_SETS, LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                r'.*dimension_sets is no longer supported.*'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_name_causes_error(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_NAME, LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                r'.*\bname field is set\b.*'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolate_name_causes_error(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ISOLATE_NAME, LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                r'.*\bisolate_name field is set\b.*'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

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
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_reusing_gtest_targets(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_load_multiple_isolate_map_files_with_duplicates(self):
    self.args.isolate_map_files = [self.args.gn_isolate_map_pyl_path]
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
    isolate_map_1 = fbb.load_pyl_file(self.args.gn_isolate_map_pyl_path)
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
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_arg_merges(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_MODIFICATIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_enable_features_arg_merges(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WITH_ENABLE_FEATURES_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES, LUCI_MILO_CFG)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_linux_args(self):
    fbb = FakeBBGen(self.args, FOO_LINUX_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_LINUX_ARGS, LUCI_MILO_CFG)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_filtering(self):
    fbb = FakeBBGen(self.args,
                    COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_modifications(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_MODIFICATIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_with_explicit_none(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_EXPLICIT_NONE_EXCEPTIONS,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolated_script_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_isolated_script_tests_android(self):
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL_ANDROID,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script_with_args(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL,
                    SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=SCRIPT_WITH_ARGS_EXCEPTIONS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_script(self):
    fbb = FakeBBGen(self.args,
                    FOO_SCRIPT_WATERFALL,
                    FOO_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
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

  def test_gpu_telemetry_test_with_invalid_name(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST_WITH_INVALID_NAME,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'telemetry test names must end with test or tests.*'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_android(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_android_webview(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_ANDROID_WEBVIEW,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_ANDROID_WEBVIEW)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_fuchsia(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_FUCHSIA,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_FUCHSIA)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_cast_streaming(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_CAST_STREAMING,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP_CAST_STREAMING)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_skylab(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_WATERFALL_SKYLAB,
                    COMPOSITION_SUITE_WITH_TELEMETRY_TEST,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_nvidia_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    NVIDIA_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_intel_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    INTEL_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gpu_telemetry_tests_with_variants(self):
    fbb = FakeBBGen(self.args,
                    GPU_TELEMETRY_TEST_VARIANTS_WATERFALL,
                    MATRIX_COMPOUND_MIXED_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP,
                    variants=MULTI_VARIANTS_FILE)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_intel_uhd_gpu_telemetry_tests(self):
    fbb = FakeBBGen(self.args,
                    INTEL_UHD_GPU_TELEMETRY_TEST_WATERFALL,
                    COMPOSITION_SUITE_WITH_GPU_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_gtest_as_isolated_Script(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    GTEST_AS_ISOLATED_SCRIPT_SUITE,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_ungenerated_output_files_are_caught(self):
    fbb = FakeBBGen(self.args,
                    COMPOSITION_GTEST_SUITE_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    exceptions=NO_BAR_TEST_EXCEPTIONS)
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True, dump=False)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(
        joined_lines, 'File chromium.test.json did not have the following'
        ' expected contents:.*')
    self.assertRegex(joined_lines, r'.*--- expected.*')
    self.assertRegex(joined_lines, r'.*\+\+\+ current.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_android_output_options(self):
    fbb = FakeBBGen(self.args, ANDROID_WATERFALL, FOO_TEST_SUITE, LUCI_MILO_CFG)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_android_swarming(self):
    fbb = FakeBBGen(self.args, ANDROID_WATERFALL,
                    FOO_TEST_SUITE_ANDROID_SWARMING, LUCI_MILO_CFG)
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
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_chromeos_trigger_script_output(self):
    fbb = FakeBBGen(self.args, FOO_CHROMEOS_TRIGGER_SCRIPT_WATERFALL,
                    FOO_TEST_SUITE, LUCI_MILO_CFG)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_relative_pyl_file_dir(self):
    self.set_args('--pyl-files-dir=relative/path')
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    REUSING_TEST_WITH_DIFFERENT_NAME,
                    LUCI_MILO_CFG,
                    gn_isolate_map=GN_ISOLATE_MAP)
    fbb.check_input_file_consistency(verbose=True)
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
        ('The following files have invalid keys: ' +
         re.escape(self.args.waterfalls_pyl_path)),
    ):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, r'.*\+ chromium\..*test.*')
    self.assertRegex(joined_lines, r'.*\- chromium\..*test.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

    fbb = FakeBBGen(self.args, TEST_SUITE_UNSORTED_WATERFALL_2,
                    TEST_SUITE_SORTED, LUCI_MILO_CFG_WATERFALL_SORTING)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        ('The following files have invalid keys: ' +
         re.escape(self.args.waterfalls_pyl_path)),
    ):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = ' '.join(fbb.printed_lines)
    self.assertRegex(joined_lines, r'.*\+.*Fake Tester.*')
    self.assertRegex(joined_lines, r'.*\-.*Fake Tester.*')
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
    self.assertRegex(joined_lines, r'.*\- Fake Tester.*')
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
    self.assertRegex(joined_lines, r'.*\+ Fake Tester.*')
    self.assertRegex(joined_lines, r'.*\- Fake Tester.*')
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
    self.assertRegex(joined_lines, r'.*\+ suite_.*')
    self.assertRegex(joined_lines, r'.*\- suite_.*')
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
      self.assertRegex(joined_lines, r'.*\+ suite_.*')
      self.assertRegex(joined_lines, r'.*\- suite_.*')
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
        'swarming': {
          'dimensions': {
            'os': 'Linux',
          },
        },
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
          'dimensions': {
            'kvm': '1',
          },
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
          'dimensions': {
            'integrity': 'high',
            'os': 'Linux',
          },
          'expiration': 120,
        },
        'mixins': ['test_mixin'],
      },
    },
  },
}
"""

FOO_TEST_SUITE_WITH_TEST_COMMON = """\
{
  'basic_suites': {
    'foo_tests': {
      'foo_test': {
        'test_common': {
          'args': ['test-common-arg'],
          'mixins': ['test-common-mixin'],
        },
        'args': ['test-arg'],
        'mixins': ['test-mixin'],
      },
    },
  },
}
"""

MIXIN_ARGS = """\
{
  'builder_mixin': {
    'args': [],
  },
}
"""

MIXIN_ARGS_NOT_LIST = """\
{
  'builder_mixin': {
    'args': 'I am not a list',
  },
}
"""

MIXIN_LINUX_ARGS = """\
{
  'builder_mixin': {
    'args': [ '--mixin-argument' ],
    'linux_args': [ '--linux-mixin-argument' ],
  },
}
"""

MIXIN_APPEND = """\
{
  'builder_mixin': {
    '$mixin_append': {
      'args': [ '--mixin-argument' ],
    },
  },
}
"""

MIXINS_FAIL_IF_UNUSED_FALSE = """\
{
  'test_mixin': {
    'fail_if_unused': False,
    'swarming': {
      'value': 'test',
    },
  },
  'unused_mixin': {
    'fail_if_unused': False,
    'args': ['--unused'],
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

SWARMING_NAMED_CACHES = """\
{
  'builder_mixin': {
    'swarming': {
      'named_caches': [
        {
          'name': 'cache',
          'file': 'cache_file',
        },
      ],
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

TEST_COMMON_MIXINS = """\
{
  'test-common-mixin': {
    'args': ['test-common-mixin-arg'],
  },
  'test-mixin': {
    'args': ['test-mixin-arg'],
  },
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
    self.assertRegex(joined_lines, r'.*\+ ._mixin.*')
    self.assertRegex(joined_lines, r'.*\- ._mixin.*')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_waterfall(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_waterfall_exception_overrides(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=SCRIPT_WITH_ARGS_SWARMING_EXCEPTIONS,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_autoshard_exceptions(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    exceptions=SCRIPT_WITH_ARGS_SWARMING_EXCEPTIONS,
                    autoshard_exceptions=AUTOSHARD_EXCEPTIONS,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_autoshard_exceptions_variant_names(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TEST_WITH_TEST_KEY,
                    LUCI_MILO_CFG,
                    autoshard_exceptions=AUTOSHARD_EXCEPTIONS,
                    variants=VARIANTS_FILE)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder_non_swarming(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_NON_SWARMING_WATERFALL,
                    FOO_TEST_SUITE,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_suite(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_DIMENSIONS_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_dimension_gpu(self):
    fbb = FakeBBGen(self.args,
                    FOO_GPU_TELEMETRY_TEST_DIMENSIONS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS,
                    gn_isolate_map=GPU_TELEMETRY_GN_ISOLATE_MAP)
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
    with self.assertRaises(generate_buildbot_json.BBGenErr):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_fail_if_unused_false(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=MIXINS_FAIL_IF_UNUSED_FALSE)
    fbb.check_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_list(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_INVALID_LIST_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
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
        ('The following files have invalid keys: ' +
         re.escape(self.args.mixins_pyl_path)),
    ):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, r'.*\- builder_mixin')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_no_duplicate_keys_basic_test_suite(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, FOO_TEST_SUITE_NOT_SORTED,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        ('The following files have invalid keys: ' +
         re.escape(self.args.test_suites_pyl_path)),
    ):
      fbb.check_input_file_consistency(verbose=True)
    joined_lines = '\n'.join(fbb.printed_lines)
    self.assertRegex(joined_lines, r'.*\- a_test')
    self.assertRegex(joined_lines, r'.*\+ a_test')
    fbb.printed_lines = []
    self.assertFalse(fbb.printed_lines)

  def test_type_assert_printing_help(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL, TEST_SUITES_SYNTAX_ERROR,
                    LUCI_MILO_CFG)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        f'Invalid \\.pyl file '
        f"'{re.escape(self.args.test_suites_pyl_path)}'.*",
    ):
      fbb.check_input_file_consistency(verbose=True)
    self.assertEqual(fbb.printed_lines, [
        f'== {self.args.test_suites_pyl_path} ==',
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

  def test_mixin_append(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=MIXIN_APPEND)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                r'.*\$mixin_append is no longer supported.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_swarming_named_caches(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_SWARMING_NAMED_CACHES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_NAMED_CACHES)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_args_field_not_list(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=MIXIN_ARGS_NOT_LIST)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                '"args" must be a list'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_args_field_merging(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=MIXIN_ARGS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_linux_args_field_merging(self):
    fbb = FakeBBGen(self.args,
                    FOO_LINUX_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    mixins=MIXIN_LINUX_ARGS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_remove_mixin_test_remove_waterfall(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_REMOVE_WATERFALL_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_remove_mixin_test_remove_builder(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_BUILDER_MIXIN_WATERFALL,
                    FOO_TEST_SUITE_WITH_REMOVE_BUILDER_MIXIN,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_test_common(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_TEST_COMMON,
                    LUCI_MILO_CFG,
                    mixins=TEST_COMMON_MIXINS)
    fbb.check_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)


TEST_SUITE_WITH_PARAMS = """\
{
  'basic_suites': {
    'bar_tests': {
      'bar_test': {
        'args': ['--no-xvfb'],
        'swarming': {
          'dimensions': {
            'device_os': 'NMF26U'
          },
        },
        'should_retry_with_patch': False,
      },
      'bar_test_test': {
        'swarming': {
          'dimensions': {
            'kvm': '1'
          },
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
          'dimensions': {
            'device_os': 'MMB29Q'
          },
          'hard_timeout': 1800
        }
      },
      'pls': {
        'swarming': {
        },
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
    'Fake Android M Tester': {
        'gtest_tests': [{
            'name': 'foo_test',
            'test': 'foo_test',
        }]
    },
    'Fake Android L Tester': {
        'gtest_tests': [{
            'test':
            'foo_test',
            'args': [
                '--gs-results-bucket=chromium-result-details',
                '--recover-devices'
            ],
            'merge': {
                'script': '//testing/merge_scripts/standard_gtest_merge.py'
            },
            'name':
            'foo_test',
            'swarming': {
                'dimensions': {
                    'device_os': 'LMY41U',
                    'device_os_type': 'user',
                    'device_type': 'hammerhead',
                    'os': 'Android'
                },
            }
        }]
    },
    'Fake Android K Tester': {
        'additional_compile_targets': ['bar_test'],
        'gtest_tests': [{
            'test':
            'foo_test',
            'args': [
                '--gs-results-bucket=chromium-result-details',
                '--recover-devices'
            ],
            'merge': {
                'script': '//testing/merge_scripts/standard_gtest_merge.py'
            },
            'name':
            'foo_test',
            'swarming': {
                'dimensions': {
                    'device_os': 'KTU84P',
                    'device_os_type': 'userdebug',
                    'device_type': 'hammerhead',
                    'os': 'Android',
                },
            }
        }]
    },
    'Android Builder': {
        'additional_compile_targets': ['bar_test']
    }
}
TEST_QUERY_BOTS_TESTS_OUTPUT = {
    'Fake Android M Tester': [{
        'name': 'foo_test',
        'test': 'foo_test',
    }],
    'Fake Android L Tester': [{
        'test':
        'foo_test',
        'args':
        ['--gs-results-bucket=chromium-result-details', '--recover-devices'],
        'merge': {
            'script': '//testing/merge_scripts/standard_gtest_merge.py'
        },
        'name':
        'foo_test',
        'swarming': {
            'dimensions': {
                'device_os': 'LMY41U',
                'device_os_type': 'user',
                'device_type': 'hammerhead',
                'os': 'Android'
            },
        }
    }],
    'Android Builder': [],
    'Fake Android K Tester': [{
        'test':
        'foo_test',
        'args':
        ['--gs-results-bucket=chromium-result-details', '--recover-devices'],
        'merge': {
            'script': '//testing/merge_scripts/standard_gtest_merge.py'
        },
        'name':
        'foo_test',
        'swarming': {
            'dimensions': {
                'device_os': 'KTU84P',
                'device_os_type': 'userdebug',
                'device_type': 'hammerhead',
                'os': 'Android'
            },
        }
    }]
}

TEST_QUERY_BOT_OUTPUT = {
    'additional_compile_targets': ['bar_test'],
    'gtest_tests': [
        {
            'test':
            'foo_test',
            'args': [
                '--gs-results-bucket=chromium-result-details',
                '--recover-devices',
            ],
            'merge': {
                'script': '//testing/merge_scripts/standard_gtest_merge.py',
            },
            'name':
            'foo_test',
            'swarming': {
                'dimensions': {
                    'device_os': 'KTU84P',
                    'device_os_type': 'userdebug',
                    'device_type': 'hammerhead',
                    'os': 'Android',
                },
            },
        },
    ],
}
TEST_QUERY_BOT_TESTS_OUTPUT = [{
    'test':
    'foo_test',
    'args':
    ['--gs-results-bucket=chromium-result-details', '--recover-devices'],
    'merge': {
        'script': '//testing/merge_scripts/standard_gtest_merge.py'
    },
    'name':
    'foo_test',
    'swarming': {
        'dimensions': {
            'device_os': 'LMY41U',
            'device_os_type': 'user',
            'device_type': 'hammerhead',
            'os': 'Android'
        },
    }
}]

TEST_QUERY_TESTS_OUTPUT = {
    'bar_test': {
        'name': 'bar_test',
        'swarming': {
            'dimensions': {
                'os': 'Linux'
            },
        }
    },
    'foo_test': {
        'name': 'foo_test',
        'swarming': {
            'dimensions': {
                'os': 'Linux'
            },
        }
    }
}

TEST_QUERY_TESTS_MULTIPLE_PARAMS_OUTPUT = ['foo_test']

TEST_QUERY_TESTS_DIMENSION_PARAMS_OUTPUT = ['bar_test']

TEST_QUERY_TESTS_SWARMING_PARAMS_OUTPUT = ['bar_test_test']

TEST_QUERY_TESTS_PARAMS_OUTPUT = ['bar_test_test']

TEST_QUERY_TESTS_PARAMS_FALSE_OUTPUT = ['bar_test']

TEST_QUERY_TEST_OUTPUT = {
    'name': 'foo_test',
    'swarming': {
        'dimensions': {
            'os': 'Linux',
        },
    },
}

TEST_QUERY_TEST_BOTS_OUTPUT = [
    'Fake Android K Tester',
    'Fake Android L Tester',
    'Fake Android M Tester',
]

TEST_QUERY_TEST_BOTS_ISOLATED_SCRIPTS_OUTPUT = ['Fake Tester']

TEST_QUERY_TEST_BOTS_NO_BOTS_OUTPUT = []


class QueryTests(TestCase):
  """Tests for the query feature."""
  def test_query_bots(self):
    self.set_args('--query=bots')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOTS_OUTPUT)

  def test_query_bots_invalid(self):
    self.set_args('--query=bots/blah/blah')
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
    self.set_args('--query=bots', '--json=result.json')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    self.assertFalse(fbb.printed_lines)

  def test_query_bots_tests(self):
    self.set_args('--query=bots/tests')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOTS_TESTS_OUTPUT)

  def test_query_invalid_bots_tests(self):
    self.set_args('--query=bots/tdfjdk')
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
    self.set_args('--query=bot/Fake Android K Tester')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.maxDiff = None  # pragma pylint: disable=attribute-defined-outside-init
    self.assertEqual(query_json, TEST_QUERY_BOT_OUTPUT)

  def test_query_bot_invalid_id(self):
    self.set_args('--query=bot/bot1')
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
    self.set_args('--query=bot/Fake Android K Tester/blah/blah')
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
    self.set_args('--query=bot/Fake Android K Tester/blahs')
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
    self.set_args('--query=bot/Fake Android L Tester/tests')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_BOT_TESTS_OUTPUT)

  def test_query_tests(self):
    self.set_args('--query=tests')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_OUTPUT)

  def test_query_tests_invalid(self):
    self.set_args('--query=tests/blah/blah')
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
    self.set_args('--query=tests/--jobs=1&--verbose')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_MULTIPLE_PARAMS_OUTPUT)

  def test_query_tests_invalid_params(self):
    self.set_args('--query=tests/device_os?')
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
    self.set_args('--query=tests/device_os:NMF26U')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_DIMENSION_PARAMS_OUTPUT)

  def test_query_tests_swarming_params(self):
    self.set_args('--query=tests/hard_timeout:1000')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_SWARMING_PARAMS_OUTPUT)

  def test_query_tests_params(self):
    self.set_args('--query=tests/should_retry_with_patch:true')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_PARAMS_OUTPUT)

  def test_query_tests_params_false(self):
    self.set_args('--query=tests/should_retry_with_patch:false')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    TEST_SUITE_WITH_PARAMS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TESTS_PARAMS_FALSE_OUTPUT)

  def test_query_test(self):
    self.set_args('--query=test/foo_test')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_OUTPUT)

  def test_query_test_invalid_id(self):
    self.set_args('--query=test/foo_foo')
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
    self.set_args('--query=test/foo_tests/foo/foo')
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
    self.set_args('--query=test/foo_test/bots')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_OUTPUT)

  def test_query_test_bots_isolated_scripts(self):
    self.set_args('--query=test/foo_test/bots')
    fbb = FakeBBGen(self.args,
                    FOO_ISOLATED_SCRIPTS_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_ISOLATED_SCRIPTS_OUTPUT)

  def test_query_test_bots_invalid(self):
    self.set_args('--query=test/foo_tests/foo')
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
    self.set_args('--query=test/bar_tests/bots')
    fbb = FakeBBGen(self.args,
                    ANDROID_WATERFALL,
                    GOOD_COMPOSITION_TEST_SUITES,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS_SORTED)
    fbb.query(fbb.args)
    query_json = json.loads(''.join(fbb.printed_lines))
    self.assertEqual(query_json, TEST_QUERY_TEST_BOTS_NO_BOTS_OUTPUT)

  def test_query_invalid(self):
    self.set_args('--query=foo')
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


class ReplacementTests(TestCase):
  """Tests for the arg replacement feature."""
  def test_replacement_valid_remove_no_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ARGS,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REMOVE_NO_VALUE)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_remove_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REMOVE_VALUE)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_replace_value(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REPLACE_VALUE)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_replacement_valid_replace_value_separate_entries(self):
    fbb = FakeBBGen(self.args,
                    FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_ENABLE_FEATURES_SEPARATE_ENTRIES,
                    LUCI_MILO_CFG,
                    exceptions=FOO_TEST_REPLACEMENTS_REPLACE_VALUE)
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


class MagicSubstitutionTests(TestCase):
  """Tests for the magic substitution feature."""
  def test_valid_function(self):
    fbb = FakeBBGen(self.args, FOO_GTESTS_WATERFALL,
                    FOO_TEST_SUITE_WITH_MAGIC_ARGS, LUCI_MILO_CFG)
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

MATRIX_COMPOUND_EMPTY_WITH_DESCRIPTION = """\
{
  'basic_suites': {
    'bar_tests': {
      'bar_test': {
        'description': 'This is a bar test',
      },
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
          'missing-identifier',
        ],
      },
    },
  },
}
"""

VARIANTS_FILE_MISSING_IDENTIFIER = """\
{
  'missing-identifier': {}
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
          'empty-identifier',
        ],
      },
    },
  },
}
"""

EMPTY_IDENTIFIER_VARIANTS = """\
{
  'empty-identifier': {
    'identifier': '',
    'swarming': {
      'dimensions': {
        'foo': 'empty identifier not allowed',
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
          'trailing-whitespace',
        ],
      },
    },
  },
}
"""

TRAILING_WHITESPACE_VARIANTS = """\
{
  'trailing-whitespace': {
    'identifier': 'id ',
    'swarming': {
      'dimensions': {
        'foo': 'trailing whitespace not allowed',
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
          'a_variant',
        ],
      },
      'foo_tests': {
        'variants': [
          'a_variant',
        ]
      }
    },
  },
}
"""

MATRIX_COMPOUND_EMPTY_VARIANTS = """\
{
  'basic_suites': {
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
      'foo_tests': {
        'variants': [],
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
          'an-arg',
        ],
      },
    },
  },
}
"""

ARGS_VARIANTS_FILE = """\
{
  'an-arg': {
    'identifier': 'args',
    'args': [
      '--anarg',
    ],
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
          'mixins',
        ],
      },
    },
  },
}
"""

MIXINS_VARIANTS_FILE = """\
{
  'mixins': {
    'identifier': 'mixins',
    'mixins': [ 'dimension_mixin' ],
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
          'dimensions': {
            'foo': 'bar',
          },
        },
      },
    }
  },
  'matrix_compound_suites': {
    'matrix_tests': {
      'foo_tests': {
        'variants': [
          'swarming-variant',
        ],
      },
    },
  },
}
"""

SWARMING_VARIANTS_FILE = """\
{
  'swarming-variant': {
    'identifier': 'swarming',
    'swarming': {
      'a': 'b',
      'dimensions': {
        'hello': 'world',
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

MATRIX_COMPOUND_VARIANTS_REF_WITH_DESCRIPTION = """\
{
  'basic_suites': {
    'foo_tests': {
      'swarming_test': {
        'description': 'This is a swarming test.'
      },
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
          'b_variant',
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

VARIANTS_FILE_WITH_DESCRIPTION = """\
{
  'a_variant': {
    'identifier': 'a_variant',
    'description': 'Variant description.'
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

MATRIX_SKYLAB_WATERFALL_WITH_NO_CROS_BOARD = """\
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
        'cros_board': 'octopus',
        'cros_dut_pool': 'chromium',
        'run_cft': True,
      },
    },
  },
]
"""

MATRIX_SKYLAB_WATERFALL_WITH_BUILD_TARGET_VARIANT = """\
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
        'cros_board': 'octopus',
        'cros_build_target': 'octopus-arc-t',
        'cros_dut_pool': 'chromium',
        'run_cft': True,
      },
    },
  },
]
"""

MATRIX_COMPOUND_SKYLAB_REF = """\
{
  'basic_suites': {
    'autotest_suites': {
      'autotest_suite': {
      },
    },
    'cros_skylab_basic': {
      'benchmark_suite': {
        'benchmark': 'something',
      },
      'chrome_all_tast_tests': {
        'tast_expr': 'dummy expr',
        'timeout': 3600,
      },
      'gtest_suite': { },
      'lacros_all_tast_tests': {
        'tast_expr': 'lacros expr',
        'timeout': 3600,
      },
    },
  },
  'compound_suites': {},
  'matrix_compound_suites': {
    'cros_skylab_basic_x86': {
      'autotest_suites': {
        'variants': [
          'octopus-89-with-autotest-name',
        ],
      },
      'cros_skylab_basic': {
        'tast_expr': 'dummy expr',
        'variants': [
          'octopus-89',
          'octopus-88',
        ],
      },
    },
  },
}
"""

SKYLAB_VARIANTS = """\
{
  'octopus-89': {
    'skylab': {
      'cros_board': 'octopus',
      'cros_model': 'casta',
      'cros_chrome_version': '89.0.3234.0',
      'cros_img': 'octopus-release/R89-13655.0.0',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT',
  },
  'octopus-89-with-autotest-name': {
    'skylab': {
      'cros_chrome_version': '89.0.3234.0',
      'cros_img': 'octopus-release/R89-13655.0.0',
      'autotest_name': 'unique_autotest_name',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT',
  },
  'octopus-88': {
    'skylab': {
      'cros_chrome_version': '88.0.2324.0',
      'cros_img': 'octopus-release/R88-13597.23.0',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT-1',
  },
}
"""

SKYLAB_VARIANTS_WITH_BUILD_VARIANT = """\
{
  'octopus-89': {
    'skylab': {
      'cros_board': 'octopus',
      'cros_model': 'casta',
      'cros_chrome_version': '89.0.3234.0',
      'cros_img': 'octopus-arc-t-release/R89-13655.0.0',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT',
  },
  'octopus-89-with-autotest-name': {
    'skylab': {
      'cros_chrome_version': '89.0.3234.0',
      'cros_img': 'octopus-arc-t-release/R89-13655.0.0',
      'autotest_name': 'unique_autotest_name',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT',
  },
  'octopus-88': {
    'skylab': {
      'cros_chrome_version': '88.0.2324.0',
      'cros_img': 'octopus-arc-t-release/R88-13597.23.0',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT-1',
  },
}
"""

ENABLED_AND_DISABLED_MATRIX_COMPOUND_SKYLAB_REF = """\
{
  'basic_suites': {
    'cros_skylab_basic': {
      'tast.basic': {
        'tast_expr': 'dummy expr',
        'suite': 'tast.basic',
        'shard_level_retries_on_ctp': 2,
        'timeout': 3600,
      },
      'tast.foo': {
        'tast_expr': 'dummy expr',
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
          'enabled',
          'disabled',
        ],
      },
    },
  },
}
"""

ENABLED_AND_DISABLED_VARIANTS = """\
{
  'enabled': {
    'skylab': {
      'cros_board': 'octopus',
      'cros_chrome_version': '89.0.3234.0',
      'cros_img': 'octopus-release/R89-13655.0.0',
    },
    'enabled': True,
    'identifier': 'OCTOPUS_TOT',
  },
  'disabled': {
    'skylab': {
      'cros_board': 'octopus',
      'cros_chrome_version': '88.0.2324.0',
      'cros_img': 'octopus-release/R88-13597.23.0',
    },
    'enabled': False,
    'identifier': 'OCTOPUS_TOT-1',
  },
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
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_structure_with_description(self):
    """
    Tests matrix compound test suite structure with description.
    """
    fbb = FakeBBGen(self.args, MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_EMPTY_WITH_DESCRIPTION, LUCI_MILO_CFG)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_missing_identifier(self):
    """
    Variant is missing an identifier
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_MISSING_IDENTIFIER,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE_MISSING_IDENTIFIER)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Missing required identifier field in matrix compound suite*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_empty_identifier(self):
    """
    Variant identifier is empty.
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_EMPTY_IDENTIFIER,
                    LUCI_MILO_CFG,
                    variants=EMPTY_IDENTIFIER_VARIANTS)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Identifier field can not be "" in matrix compound suite*'):
      fbb.check_output_file_consistency(verbose=True)

  def test_trailing_identifier(self):
    """
    Variant identifier has trailing whitespace.
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TRAILING_IDENTIFIER,
                    LUCI_MILO_CFG,
                    variants=TRAILING_WHITESPACE_VARIANTS)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'Identifier field can not have leading and trailing whitespace in'
        ' matrix compound suite*'):
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
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_CONFLICTING_TEST_SUITES,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'Conflicting test definitions.*'):
      fbb.check_input_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_empty_variants(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_EMPTY_VARIANTS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_swarming_dict_merge_args(self):
    """
    Test targets with swarming dictionary defined by both basic and matrix
    """
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_TARGETS_ARGS,
                    LUCI_MILO_CFG,
                    mixins=SWARMING_MIXINS,
                    variants=ARGS_VARIANTS_FILE)
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
                    mixins=SWARMING_MIXINS,
                    variants=MIXINS_VARIANTS_FILE)
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
                    variants=SWARMING_VARIANTS_FILE)
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
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_with_description(self):
    """Test variants with description field"""
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE_WITH_DESCRIPTION)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_both_test_suite_and_variants_with_description(self):
    """Test both test suite and variants with description field"""
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_VARIANTS_REF_WITH_DESCRIPTION,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE_WITH_DESCRIPTION)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_variants_pyl_ref(self):
    """Test targets with variants string ref"""
    fbb = FakeBBGen(self.args,
                    MATRIX_GTEST_SUITE_WATERFALL,
                    MATRIX_COMPOUND_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    variants=VARIANTS_FILE)
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
                    MATRIX_COMPOUND_VARIANTS_REF,
                    LUCI_MILO_CFG,
                    variants=MULTI_VARIANTS_FILE)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'The following variants were unreferenced *'):
      fbb.check_input_file_consistency(verbose=True)

  def test_good_skylab_matrix_with_variants(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL,
                    MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS,
                    variants=SKYLAB_VARIANTS)
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_good_skylab_matrix_with_build_target_variant_and_variants(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL_WITH_BUILD_TARGET_VARIANT,
                    MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS,
                    variants=SKYLAB_VARIANTS_WITH_BUILD_VARIANT)
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_enabled_and_disabled_skylab_matrix_with_variants(self):
    """Test with disabled variants"""
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL,
                    ENABLED_AND_DISABLED_MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS,
                    variants=ENABLED_AND_DISABLED_VARIANTS)
    # some skylab test variant is disabled; the corresponding skylab tests
    # is not generated.
    fbb.check_input_file_consistency(verbose=True)
    fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_invalid_skylab_matrix_with_variants(self):
    fbb = FakeBBGen(self.args,
                    MATRIX_SKYLAB_WATERFALL_WITH_NO_CROS_BOARD,
                    MATRIX_COMPOUND_SKYLAB_REF,
                    LUCI_MILO_CFG,
                    exceptions=EMPTY_SKYLAB_TEST_EXCEPTIONS,
                    variants=SKYLAB_VARIANTS)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'skylab tests must specify cros_board.'):
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

NO_DIMENSIONS_GTESTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Mac': {
        'swarming': {},
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
"""

NO_OS_GTESTS_WATERFALL = """\
[
  {
    'project': 'chromium',
    'bucket': 'ci',
    'name': 'chromium.test',
    'machines': {
      'Mac': {
        'swarming': {
          'dimensions': {
            'foo': 'bar',
          },
        },
        'test_suites': {
          'gtest_tests': 'foo_tests',
        },
      },
    },
  },
]
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
          'dimensions': {
            'os': 'Mac',
          },
        },
        'test_suites': {
          'isolated_scripts': 'foo_tests',
        },
      },
    },
  },
]
"""

MAC_LUCI_MILO_CFG = """\
consoles {
  builders {
    name: "buildbucket/luci.chromium.ci/Mac"
  }
}
"""


class SwarmingTests(TestCase):
  def test_builder_with_no_dimension_fails(self):
    fbb = FakeBBGen(self.args, NO_DIMENSIONS_GTESTS_WATERFALL, MAC_TEST_SUITE,
                    MAC_LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(
        generate_buildbot_json.BBGenErr,
        'dimensions must be specified for all swarmed tests'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_builder_with_no_os_dimension_fails(self):
    fbb = FakeBBGen(self.args, NO_OS_GTESTS_WATERFALL, MAC_TEST_SUITE,
                    MAC_LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'os must be specified for all swarmed tests'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)

  def test_mac_builder_with_no_cpu_dimension_in_isolated_script_fails(self):
    fbb = FakeBBGen(self.args, MAC_ISOLATED_SCRIPTS_WATERFALL, MAC_TEST_SUITE,
                    MAC_LUCI_MILO_CFG)
    fbb.check_input_file_consistency(verbose=True)
    with self.assertRaisesRegex(generate_buildbot_json.BBGenErr,
                                'cpu must be specified for mac'):
      fbb.check_output_file_consistency(verbose=True)
    self.assertFalse(fbb.printed_lines)


if __name__ == '__main__':
  unittest.main()

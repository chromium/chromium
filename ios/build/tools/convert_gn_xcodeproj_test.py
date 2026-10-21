#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for `convert_gn_xcodeproj.py`."""

import filecmp
import json
import os
import shutil
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

import convert_gn_xcodeproj

_ROOT_DIR = os.path.abspath(
  os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
)
_PROJECT_NAME = 'all.xcodeproj'

_APP_TARGET_ID = 'TARGET000000000000000001'
_APP_PRODUCT_ID = 'PROD00000000000000000001'
_APP_CONFIG_LIST_ID = 'CFGLIST00000000000000001'
_APP_CONFIG_DEBUG_ID = 'CFG000000000000000000001'

_UNIT_HOST_TARGET_ID = 'TARGET000000000000000002'
_UNIT_HOST_PRODUCT_ID = 'PROD00000000000000000002'
_UNIT_HOST_CONFIG_LIST_ID = 'CFGLIST00000000000000002'
_UNIT_HOST_CONFIG_DEBUG_ID = 'CFG000000000000000000002'

_UNIT_TEST_TARGET_ID = 'TARGET000000000000000003'
_UNIT_TEST_PRODUCT_ID = 'PROD00000000000000000003'
_UNIT_TEST_CONFIG_LIST_ID = 'CFGLIST00000000000000003'
_UNIT_TEST_CONFIG_DEBUG_ID = 'CFG000000000000000000003'

_UI_HOST_TARGET_ID = 'TARGET000000000000000004'
_UI_HOST_PRODUCT_ID = 'PROD00000000000000000004'
_UI_HOST_CONFIG_LIST_ID = 'CFGLIST00000000000000004'
_UI_HOST_CONFIG_DEBUG_ID = 'CFG000000000000000000004'

_UI_TEST_TARGET_ID = 'TARGET000000000000000005'
_UI_TEST_PRODUCT_ID = 'PROD00000000000000000005'
_UI_TEST_CONFIG_LIST_ID = 'CFGLIST00000000000000005'
_UI_TEST_CONFIG_DEBUG_ID = 'CFG000000000000000000005'


def _BuildSampleProjectData() -> dict:
  """Returns a sample Xcode project JSON dictionary as produced by GN."""
  return {
    'objects': {
      # Standalone application target (`chrome`).
      _APP_PRODUCT_ID: {
        'isa': 'PBXFileReference',
        'path': 'chrome.app',
      },
      _APP_CONFIG_DEBUG_ID: {
        'isa': 'XCBuildConfiguration',
        'name': 'Debug',
        'buildSettings': {},
      },
      _APP_CONFIG_LIST_ID: {
        'isa': 'XCConfigurationList',
        'buildConfigurations': [_APP_CONFIG_DEBUG_ID],
      },
      _APP_TARGET_ID: {
        'isa': 'PBXNativeTarget',
        'name': 'chrome',
        'productType': 'com.apple.product-type.application',
        'productReference': _APP_PRODUCT_ID,
        'buildConfigurationList': _APP_CONFIG_LIST_ID,
      },
      # Unit test host application (`ios_chrome_unittests`).
      _UNIT_HOST_PRODUCT_ID: {
        'isa': 'PBXFileReference',
        'path': 'ios_chrome_unittests.app',
      },
      _UNIT_HOST_CONFIG_DEBUG_ID: {
        'isa': 'XCBuildConfiguration',
        'name': 'Debug',
        'buildSettings': {},
      },
      _UNIT_HOST_CONFIG_LIST_ID: {
        'isa': 'XCConfigurationList',
        'buildConfigurations': [_UNIT_HOST_CONFIG_DEBUG_ID],
      },
      _UNIT_HOST_TARGET_ID: {
        'isa': 'PBXNativeTarget',
        'name': 'ios_chrome_unittests',
        'productType': 'com.apple.product-type.application',
        'productReference': _UNIT_HOST_PRODUCT_ID,
        'buildConfigurationList': _UNIT_HOST_CONFIG_LIST_ID,
      },
      # Unit test bundle target (`ios_chrome_unittests_module`).
      _UNIT_TEST_PRODUCT_ID: {
        'isa': 'PBXFileReference',
        'path': 'ios_chrome_unittests_module.xctest',
      },
      _UNIT_TEST_CONFIG_DEBUG_ID: {
        'isa': 'XCBuildConfiguration',
        'name': 'Debug',
        'buildSettings': {
          'TEST_HOST': (
            '$(BUILT_PRODUCTS_DIR)/ios_chrome_unittests.app/'
            'ios_chrome_unittests'
          ),
        },
      },
      _UNIT_TEST_CONFIG_LIST_ID: {
        'isa': 'XCConfigurationList',
        'buildConfigurations': [_UNIT_TEST_CONFIG_DEBUG_ID],
      },
      _UNIT_TEST_TARGET_ID: {
        'isa': 'PBXNativeTarget',
        'name': 'ios_chrome_unittests_module',
        'productType': 'com.apple.product-type.bundle.unit-test',
        'productReference': _UNIT_TEST_PRODUCT_ID,
        'buildConfigurationList': _UNIT_TEST_CONFIG_LIST_ID,
      },
      # UI test host application (`ios_chrome_eg2tests`).
      _UI_HOST_PRODUCT_ID: {
        'isa': 'PBXFileReference',
        'path': 'ios_chrome_eg2tests.app',
      },
      _UI_HOST_CONFIG_DEBUG_ID: {
        'isa': 'XCBuildConfiguration',
        'name': 'Debug',
        'buildSettings': {},
      },
      _UI_HOST_CONFIG_LIST_ID: {
        'isa': 'XCConfigurationList',
        'buildConfigurations': [_UI_HOST_CONFIG_DEBUG_ID],
      },
      _UI_HOST_TARGET_ID: {
        'isa': 'PBXNativeTarget',
        'name': 'ios_chrome_eg2tests',
        'productType': 'com.apple.product-type.application',
        'productReference': _UI_HOST_PRODUCT_ID,
        'buildConfigurationList': _UI_HOST_CONFIG_LIST_ID,
      },
      # UI test bundle target (`ios_chrome_ui_eg2tests_module`).
      _UI_TEST_PRODUCT_ID: {
        'isa': 'PBXFileReference',
        'path': 'ios_chrome_ui_eg2tests_module.xctest',
      },
      _UI_TEST_CONFIG_DEBUG_ID: {
        'isa': 'XCBuildConfiguration',
        'name': 'Debug',
        'buildSettings': {
          'TEST_TARGET_NAME': 'ios_chrome_eg2tests',
        },
      },
      _UI_TEST_CONFIG_LIST_ID: {
        'isa': 'XCConfigurationList',
        'buildConfigurations': [_UI_TEST_CONFIG_DEBUG_ID],
      },
      _UI_TEST_TARGET_ID: {
        'isa': 'PBXNativeTarget',
        'name': 'ios_chrome_ui_eg2tests_module',
        'productType': 'com.apple.product-type.bundle.ui-testing',
        'productReference': _UI_TEST_PRODUCT_ID,
        'buildConfigurationList': _UI_TEST_CONFIG_LIST_ID,
      },
    },
  }


class ConvertGnXcodeprojTest(unittest.TestCase):
  """Tests for `convert_gn_xcodeproj.py`."""

  def setUp(self) -> None:
    self.temp_dir = tempfile.mkdtemp()
    self.input_dir = os.path.join(self.temp_dir, 'input')
    self.output_dir = os.path.join(self.temp_dir, 'output')
    self.input_proj_dir = os.path.join(self.input_dir, _PROJECT_NAME)
    self.output_proj_dir = os.path.join(self.output_dir, _PROJECT_NAME)
    os.makedirs(self.input_proj_dir)

    self.sample_data = _BuildSampleProjectData()
    self.pbxproj_content = '// !$*UTF8*$!\n{\n  sample = project;\n}\n'
    self.input_pbxproj = os.path.join(self.input_proj_dir, 'project.pbxproj')
    with open(self.input_pbxproj, 'w', encoding='utf-8') as f:
      f.write(self.pbxproj_content)

  def tearDown(self) -> None:
    shutil.rmtree(self.temp_dir)

  def testGetHostMappingForXCTests(self) -> None:
    """Tests that `GetHostMappingForXCTests` maps unit and UI test targets."""
    project = convert_gn_xcodeproj.XcodeProject(self.sample_data['objects'])
    mapping = project.GetHostMappingForXCTests()

    self.assertEqual(
      mapping,
      {
        'ios_chrome_unittests.app': [
          (
            _UNIT_TEST_TARGET_ID,
            'ios_chrome_unittests_module',
            'ios_chrome_unittests_module.xctest',
          )
        ],
        'ios_chrome_eg2tests': [
          (
            _UI_TEST_TARGET_ID,
            'ios_chrome_ui_eg2tests_module',
            'ios_chrome_ui_eg2tests_module.xctest',
          )
        ],
      },
    )

  @mock.patch('convert_gn_xcodeproj.check_output')
  def testConvertGnXcodeProjectGeneratesSchemesAndPreservesPbxproj(
    self, mock_check_output: mock.MagicMock
  ) -> None:
    """Tests that schemes are generated without modifying `project.pbxproj`."""
    mock_check_output.return_value = json.dumps(self.sample_data)

    convert_gn_xcodeproj.ConvertGnXcodeProject(
      _ROOT_DIR,
      _PROJECT_NAME,
      self.input_dir,
      self.output_dir,
    )

    # Verify that `plutil` was the only external command invoked (no
    # `git ls-files`).
    mock_check_output.assert_called_once_with(
      [
        'plutil',
        '-convert',
        'json',
        '-o',
        '-',
        self.input_pbxproj,
      ]
    )

    # Verify that `project.pbxproj` is copied unchanged to `output_dir`.
    output_pbxproj = os.path.join(self.output_proj_dir, 'project.pbxproj')
    self.assertTrue(os.path.exists(output_pbxproj))
    self.assertTrue(
      filecmp.cmp(self.input_pbxproj, output_pbxproj, shallow=False)
    )

    # Verify generated scheme files.
    schemes_dir = os.path.join(
      self.output_proj_dir, 'xcshareddata', 'xcschemes'
    )
    self.assertEqual(
      sorted(os.listdir(schemes_dir)),
      sorted(
        [
          'chrome.xcscheme',
          'ios_chrome_unittests_module.xcscheme',
          'ios_chrome_ui_eg2tests_module.xcscheme',
        ]
      ),
    )

    # Verify non-test app scheme contents.
    chrome_scheme = ET.parse(os.path.join(schemes_dir, 'chrome.xcscheme'))
    chrome_root = chrome_scheme.getroot()
    launch_action = chrome_root.find('LaunchAction')
    self.assertIsNotNone(launch_action)
    self.assertEqual(
      launch_action.get('customLLDBInitFile'),
      convert_gn_xcodeproj.LLDBINIT_PATH,
    )

    # Verify UI test scheme associates with `ios_chrome_eg2tests` host app.
    ui_test_scheme = ET.parse(
      os.path.join(schemes_dir, 'ios_chrome_ui_eg2tests_module.xcscheme')
    )
    ui_test_root = ui_test_scheme.getroot()
    ui_launch_ref = ui_test_root.find(
      './LaunchAction/BuildableProductRunnable/BuildableReference'
    )
    self.assertIsNotNone(ui_launch_ref)
    self.assertEqual(ui_launch_ref.get('BlueprintName'), 'ios_chrome_eg2tests')
    self.assertEqual(
      ui_launch_ref.get('BuildableName'), 'ios_chrome_eg2tests.app'
    )

  @mock.patch('convert_gn_xcodeproj.check_output')
  def testConvertGnXcodeProjectUpdatesExistingSchemes(
    self, mock_check_output: mock.MagicMock
  ) -> None:
    """Tests that pre-existing schemes in `output_dir` are updated."""
    mock_check_output.return_value = json.dumps(self.sample_data)

    # First run populates `output_dir`.
    convert_gn_xcodeproj.ConvertGnXcodeProject(
      _ROOT_DIR,
      _PROJECT_NAME,
      self.input_dir,
      self.output_dir,
    )

    # Modify an existing scheme in `output_dir` to have a stale
    # `customLLDBInitFile`.
    scheme_path = os.path.join(
      self.output_proj_dir,
      'xcshareddata',
      'xcschemes',
      'ios_chrome_ui_eg2tests_module.xcscheme',
    )
    tree = ET.parse(scheme_path)
    test_action = tree.getroot().find('TestAction')
    self.assertIsNotNone(test_action)
    test_action.set('customLLDBInitFile', 'stale_path')
    test_action.set('shouldUseLaunchSchemeArgsEnv', 'NO')
    tree.write(scheme_path, xml_declaration=True, encoding='UTF-8')

    # Re-create a fresh `input_dir` without schemes and run again against
    # existing `output_dir`.
    shutil.rmtree(os.path.join(self.input_proj_dir, 'xcshareddata'))
    convert_gn_xcodeproj.ConvertGnXcodeProject(
      _ROOT_DIR,
      _PROJECT_NAME,
      self.input_dir,
      self.output_dir,
    )

    updated_tree = ET.parse(scheme_path)
    updated_test_action = updated_tree.getroot().find('TestAction')
    self.assertIsNotNone(updated_test_action)
    self.assertEqual(
      updated_test_action.get('customLLDBInitFile'),
      convert_gn_xcodeproj.LLDBINIT_PATH,
    )
    self.assertEqual(
      updated_test_action.get('shouldUseLaunchSchemeArgsEnv'), 'YES'
    )


if __name__ == '__main__':
  unittest.main()

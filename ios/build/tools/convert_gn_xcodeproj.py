#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Xcode scheme files for GN-generated Xcode projects."""

import argparse
import collections
import filecmp
import functools
import hashlib
import io
import json
import os
import shutil
import string
import subprocess
import sys
import xml.etree.ElementTree


LLDBINIT_PATH = '$(PROJECT_DIR)/.lldbinit'

XCTEST_PRODUCT_TYPES = frozenset(
  (
    'com.apple.product-type.bundle.unit-test',
    'com.apple.product-type.bundle.ui-testing',
  )
)

SCHEME_PRODUCT_TYPES = frozenset(
  (
    'com.apple.product-type.app-extension',
    'com.apple.product-type.application',
    'com.apple.product-type.framework',
  )
)


class Template(string.Template):
  """A subclass of string.Template that changes delimiter."""

  delimiter = '@'


@functools.lru_cache
def LoadSchemeTemplate(root, name):
  """Return a string.Template object for scheme file loaded relative to root."""
  path = os.path.join(root, 'ios', 'build', 'tools', name + '.template')
  with open(path) as file:
    return Template(file.read())


def CreateIdentifier(str_id):
  """Return a 24 characters string that can be used as an identifier."""
  return hashlib.sha1(str_id.encode("utf-8")).hexdigest()[:24].upper()


def GenerateSchemeForTarget(
  root,
  project,
  old_project,
  name,
  path,
  is_test,
  test_host_name,
  test_host_path,
):
  """Generates the .xcsheme file for target named |name|.

  The file is generated in the new project schemes directory from a template.
  If there is an existing previous project, then the old scheme file is copied
  and the lldbinit setting is set. If lldbinit setting is already correct, the
  file is not modified, just copied.
  """
  project_name = os.path.basename(project)
  relative_path = os.path.join('xcshareddata', 'xcschemes', name + '.xcscheme')
  identifier = CreateIdentifier('%s %s' % (name, path))

  scheme_path = os.path.join(project, relative_path)
  if not os.path.isdir(os.path.dirname(scheme_path)):
    os.makedirs(os.path.dirname(scheme_path))

  substitutions = {
    'LLDBINIT_PATH': LLDBINIT_PATH,
    'BLUEPRINT_IDENTIFIER': identifier,
    'BUILDABLE_NAME': path,
    'BLUEPRINT_NAME': name,
    'PROJECT_NAME': project_name,
  }

  if is_test:
    host_name = test_host_name or name
    host_path = test_host_path or path
    host_identifier = CreateIdentifier('%s %s' % (host_name, host_path))
    template = LoadSchemeTemplate(root, 'xcodescheme-testable')
    substitutions['PATH'] = os.environ['PATH']
    substitutions['HOST_BLUEPRINT_NAME'] = host_name
    substitutions['HOST_BUILDABLE_NAME'] = host_path
    substitutions['HOST_BLUEPRINT_IDENTIFIER'] = host_identifier

  else:
    template = LoadSchemeTemplate(root, 'xcodescheme')

  old_scheme_path = os.path.join(old_project, relative_path)
  if os.path.exists(old_scheme_path):
    tree = xml.etree.ElementTree.parse(old_scheme_path)
    tree_root = tree.getroot()

    for reference in tree_root.findall('.//BuildableReference'):
      for attr, value in (
        ('BuildableName', path),
        ('BlueprintName', name),
        ('BlueprintIdentifier', identifier),
      ):
        if reference.get(attr) != value:
          reference.set(attr, value)

    for child in tree_root:
      if child.tag not in ('TestAction', 'LaunchAction'):
        continue

      if child.get('customLLDBInitFile') != LLDBINIT_PATH:
        child.set('customLLDBInitFile', LLDBINIT_PATH)

    if is_test:
      template_tree = xml.etree.ElementTree.parse(
        io.StringIO(template.substitute(**substitutions))
      )

      template_tree_root = template_tree.getroot()
      for child in tree_root:
        if child.tag == 'BuildAction':
          for subchild in list(child):
            child.remove(subchild)

          for post_action in template_tree_root.findall('.//PostActions'):
            child.append(post_action)
        elif child.tag == 'TestAction':
          child.set('shouldUseLaunchSchemeArgsEnv', 'YES')
        elif child.tag == 'LaunchAction':
          # Adding explicit BuildableProductRunnable to LaunchAction ensures
          # $(PROJECT_DIR) resolves when TestAction inherits from LaunchAction,
          # allowing the custom LLDB init file to load.
          runnable = child.find('BuildableProductRunnable')
          if runnable is None:
            runnable = template_tree_root.find(
              './/LaunchAction/BuildableProductRunnable'
            )
            if runnable is not None:
              child.append(runnable)
          else:
            ref = runnable.find('BuildableReference')
            if ref is not None:
              ref.set('BuildableName', host_path)
              ref.set('BlueprintName', host_name)
              ref.set('BlueprintIdentifier', host_identifier)

    tree.write(scheme_path, xml_declaration=True, encoding='UTF-8')

  else:
    with open(scheme_path, 'w') as scheme_file:
      scheme_file.write(template.substitute(**substitutions))


class XcodeProject(object):
  def __init__(self, objects):
    self.objects = objects

  def IterObjectsByIsa(self, isa):
    """Iterates overs objects of the |isa| type."""
    for key, obj in self.objects.items():
      if obj['isa'] == isa:
        yield (key, obj)

  def IterNativeTargetByProductType(self, product_types):
    """Iterates over PBXNativeTarget objects of any |product_types| types."""
    for key, obj in self.IterObjectsByIsa('PBXNativeTarget'):
      if obj['productType'] in product_types:
        yield (key, obj)

  def GetHostMappingForXCTests(self):
    """Returns a dict from targets to the list of their xctests modules."""
    mapping = collections.defaultdict(list)
    for key, obj in self.IterNativeTargetByProductType(XCTEST_PRODUCT_TYPES):
      build_config_lists_id = obj['buildConfigurationList']
      build_configs = self.objects[build_config_lists_id]['buildConfigurations']

      # Use the first build configuration to get the name of the host target.
      # This is arbitrary, but since the build configurations are all identical
      # except for their 'name', it is fine.
      build_config = self.objects[build_configs[0]]
      if obj['productType'] == 'com.apple.product-type.bundle.unit-test':
        # The test_host value will look like this:
        # `$(BUILD_PRODUCTS_DIR)/host_app_name.app/host_app_name`
        #
        # Extract the `host_app_name.app` part as key for the output.
        test_host_path = build_config['buildSettings']['TEST_HOST']
        test_host_name = os.path.basename(os.path.dirname(test_host_path))
      else:
        test_host_name = build_config['buildSettings']['TEST_TARGET_NAME']

      test_name = obj['name']
      test_path = self.objects[obj['productReference']]['path']

      mapping[test_host_name].append((key, test_name, test_path))

    return dict(mapping)


def check_output(command):
  """Wrapper around subprocess.check_output that decode output as utf-8."""
  return subprocess.check_output(command).decode('utf-8')


def CopyFileIfChanged(source_path, target_path):
  """Copy |source_path| to |target_path| if different."""
  target_dir = os.path.dirname(target_path)
  if not os.path.isdir(target_dir):
    os.makedirs(target_dir)
  if not os.path.exists(target_path) or not filecmp.cmp(
    source_path, target_path
  ):
    shutil.copyfile(source_path, target_path)


def CopyTreeIfChanged(source, target):
  """Copy |source| to |target| recursively; files are copied iff changed."""
  if os.path.isfile(source):
    return CopyFileIfChanged(source, target)
  if not os.path.isdir(target):
    os.makedirs(target)
  for name in os.listdir(source):
    CopyTreeIfChanged(os.path.join(source, name), os.path.join(target, name))


def LoadXcodeProjectAsJSON(project_dir):
  """Return Xcode project at |path| as a JSON string."""
  return check_output(
    [
      'plutil',
      '-convert',
      'json',
      '-o',
      '-',
      os.path.join(project_dir, 'project.pbxproj'),
    ]
  )


def UpdateXcodeProject(project_dir, old_project_dir, root_dir):
  """Generates schemes in the Xcode project at `project_dir`.

  Args:
    project_dir: path to the input Xcode project
    old_project_dir: path to the existing output Xcode project
    root_dir: path to the root directory used to load scheme templates
  """
  json_data = json.loads(LoadXcodeProjectAsJSON(project_dir))
  project = XcodeProject(json_data['objects'])

  mapping = project.GetHostMappingForXCTests()

  # Generate schemes for application, extensions and framework targets
  for key, obj in project.IterNativeTargetByProductType(SCHEME_PRODUCT_TYPES):
    product = project.objects[obj['productReference']]
    product_path = product['path']

    # Do not generate scheme for the XCTests and XXCUITests target app.
    # Instead, a scheme will be generated for each test modules.
    tests = mapping.get(product_path, []) + mapping.get(obj['name'], [])
    if not tests:
      GenerateSchemeForTarget(
        root_dir,
        project_dir,
        old_project_dir,
        obj['name'],
        product_path,
        False,
        "",
        "",
      )

    else:
      for _, test_name, test_path in tests:
        GenerateSchemeForTarget(
          root_dir,
          project_dir,
          old_project_dir,
          test_name,
          test_path,
          True,
          obj['name'],
          product_path,
        )


def ConvertGnXcodeProject(root_dir, proj_name, input_dir, output_dir):
  """Generates schemes for the GN Xcode project and copies it to `output_dir`.

  Args:
    root_dir: directory that is the root of the project
    proj_name: name of the Xcode project "file" (usually `all.xcodeproj`)
    input_dir: directory containing the Xcode projects created by "gn gen --ide"
    output_dir: directory where the tweaked Xcode projects will be saved
  """

  UpdateXcodeProject(
    os.path.join(input_dir, proj_name),
    os.path.join(output_dir, proj_name),
    root_dir,
  )

  CopyTreeIfChanged(
    os.path.join(input_dir, proj_name), os.path.join(output_dir, proj_name)
  )


def Main(args):
  parser = argparse.ArgumentParser(
    description='Generate schemes for GN Xcode projects for iOS.'
  )
  parser.add_argument(
    'input', help='directory containing [product|all] Xcode projects.'
  )
  parser.add_argument(
    'output', help='directory where to generate the iOS configuration.'
  )
  parser.add_argument(
    '--root',
    type=os.path.abspath,
    required=True,
    help='root directory of the project',
  )
  parser.add_argument(
    '--project-name',
    default='all.xcodeproj',
    dest='proj_name',
    help='name of the Xcode project (default: %(default)s)',
  )
  args = parser.parse_args(args)

  if not os.path.isdir(args.input):
    sys.stderr.write('Input directory does not exists.\n')
    return 1

  if args.proj_name not in os.listdir(args.input):
    sys.stderr.write('Input directory does not contain the Xcode project.\n')
    return 1

  ConvertGnXcodeProject(args.root, args.proj_name, args.input, args.output)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

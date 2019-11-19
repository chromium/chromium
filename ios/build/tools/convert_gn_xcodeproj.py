#!/usr/bin/python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert GN Xcode projects to platform and configuration independent targets.

GN generates Xcode projects that build one configuration only. However, typical
iOS development involves using the Xcode IDE to toggle the platform and
configuration. This script replaces the 'gn' configuration with 'Debug',
'Release' and 'Profile', and changes the ninja invocation to honor these
configurations.
"""

import argparse
import collections
import copy
import filecmp
import json
import hashlib
import os
import plistlib
import random
import shutil
import subprocess
import sys
import tempfile


class XcodeProject(object):

  def __init__(self, objects, counter = 0):
    self.objects = objects
    self.counter = 0

  def AddObject(self, parent_name, obj):
    while True:
      self.counter += 1
      str_id = "%s %s %d" % (parent_name, obj['isa'], self.counter)
      new_id = hashlib.sha1(str_id).hexdigest()[:24].upper()

      # Make sure ID is unique. It's possible there could be an id conflict
      # since this is run after GN runs.
      if new_id not in self.objects:
        self.objects[new_id] = obj
        return new_id


def CopyFileIfChanged(source_path, target_path):
  """Copy |source_path| to |target_path| is different."""
  target_dir = os.path.dirname(target_path)
  if not os.path.isdir(target_dir):
    os.makedirs(target_dir)
  if not os.path.exists(target_path) or \
      not filecmp.cmp(source_path, target_path):
    shutil.copyfile(source_path, target_path)


def LoadXcodeProjectAsJSON(path):
  """Return Xcode project at |path| as a JSON string."""
  return subprocess.check_output([
      'plutil', '-convert', 'json', '-o', '-', path])


def WriteXcodeProject(output_path, json_string):
  """Save Xcode project to |output_path| as XML."""
  with tempfile.NamedTemporaryFile() as temp_file:
    temp_file.write(json_string)
    temp_file.flush()
    subprocess.check_call(['plutil', '-convert', 'xml1', temp_file.name])
    CopyFileIfChanged(temp_file.name, output_path)


def UpdateProductsProject(file_input, file_output, configurations, root_dir):
  """Update Xcode project to support multiple configurations.

  Args:
    file_input: path to the input Xcode project
    file_output: path to the output file
    configurations: list of string corresponding to the configurations that
      need to be supported by the tweaked Xcode projects, must contains at
      least one value.
  """
  json_data = json.loads(LoadXcodeProjectAsJSON(file_input))
  project = XcodeProject(json_data['objects'])

  objects_to_remove = []
  for value in project.objects.values():
    isa = value['isa']

    # Teach build shell script to look for the configuration and platform.
    if isa == 'PBXShellScriptBuildPhase':
      value['shellScript'] = value['shellScript'].replace(
          'ninja -C .',
          'ninja -C "../${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}"')

    # Add new configuration, using the first one as default.
    if isa == 'XCConfigurationList':
      value['defaultConfigurationName'] = configurations[0]
      objects_to_remove.extend(value['buildConfigurations'])

      build_config_template = project.objects[value['buildConfigurations'][0]]
      build_config_template['buildSettings']['CONFIGURATION_BUILD_DIR'] = \
          '$(PROJECT_DIR)/../$(CONFIGURATION)$(EFFECTIVE_PLATFORM_NAME)'
      build_config_template['buildSettings']['CODE_SIGN_IDENTITY'] = ''

      value['buildConfigurations'] = []
      for configuration in configurations:
        new_build_config = copy.copy(build_config_template)
        new_build_config['name'] = configuration
        value['buildConfigurations'].append(
            project.AddObject('products', new_build_config))

  for object_id in objects_to_remove:
    del project.objects[object_id]

  AddMarkdownToProject(project, root_dir, json_data['rootObject'])

  objects = collections.OrderedDict(sorted(project.objects.iteritems()))
  WriteXcodeProject(file_output, json.dumps(json_data))


def AddMarkdownToProject(project, root_dir, root_object):
  list_files_cmd = ['git', '-C', root_dir, 'ls-files', '*.md']
  paths = subprocess.check_output(list_files_cmd).splitlines()
  ios_internal_dir = os.path.join(root_dir, 'ios_internal')
  if os.path.exists(ios_internal_dir):
    list_files_cmd = ['git', '-C', ios_internal_dir, 'ls-files', '*.md']
    ios_paths = subprocess.check_output(list_files_cmd).splitlines()
    paths.extend(["ios_internal/" + path for path in ios_paths])
  for path in paths:
    new_markdown_entry = {
      "fileEncoding": "4",
      "isa": "PBXFileReference",
      "lastKnownFileType": "net.daringfireball.markdown",
      "name": os.path.basename(path),
      "path": path,
      "sourceTree": "<group>"
    }
    new_markdown_entry_id = project.AddObject('sources', new_markdown_entry)
    folder = GetFolderForPath(project, root_object, os.path.dirname(path))
    folder['children'].append(new_markdown_entry_id)


def GetFolderForPath(project, rootObject, path):
  objects = project.objects
  # 'Sources' is always the first child of
  # project->rootObject->mainGroup->children.
  root = objects[objects[objects[rootObject]['mainGroup']]['children'][0]]
  if not path:
    return root
  for folder in path.split('/'):
    children = root['children']
    new_root = None
    for child in children:
      if objects[child]['isa'] == 'PBXGroup' and \
         objects[child]['name'] == folder:
        new_root = objects[child]
        break
    if not new_root:
      # If the folder isn't found we could just cram it into the leaf existing
      # folder, but that leads to folders with tons of README.md inside.
      new_group =  {
        "children": [
        ],
        "isa": "PBXGroup",
        "name": folder,
        "sourceTree": "<group>"
      }
      new_group_id = project.AddObject('sources', new_group)
      children.append(new_group_id)
      new_root = objects[new_group_id]
    root = new_root
  return root


def DisableNewBuildSystem(output_dir):
  """Disables the new build system due to crbug.com/852522 """
  xcwspacesharedsettings = os.path.join(output_dir, 'all.xcworkspace',
      'xcshareddata', 'WorkspaceSettings.xcsettings')
  if os.path.isfile(xcwspacesharedsettings):
    json_data = json.loads(LoadXcodeProjectAsJSON(xcwspacesharedsettings))
  else:
    json_data = {}
  json_data['BuildSystemType'] = 'Original'
  WriteXcodeProject(xcwspacesharedsettings, json.dumps(json_data))


def ConvertGnXcodeProject(root_dir, input_dir, output_dir, configurations):
  '''Tweak the Xcode project generated by gn to support multiple configurations.

  The Xcode projects generated by "gn gen --ide" only supports a single
  platform and configuration (as the platform and configuration are set
  per output directory). This method takes as input such projects and
  add support for multiple configurations and platforms (to allow devs
  to select them in Xcode).

  Args:
    input_dir: directory containing the XCode projects created by "gn gen --ide"
    output_dir: directory where the tweaked Xcode projects will be saved
    configurations: list of string corresponding to the configurations that
      need to be supported by the tweaked Xcode projects, must contains at
      least one value.
  '''
  # Update products project.
  products = os.path.join('products.xcodeproj', 'project.pbxproj')
  product_input = os.path.join(input_dir, products)
  product_output = os.path.join(output_dir, products)
  UpdateProductsProject(product_input, product_output, configurations, root_dir)

  # Copy all workspace.
  xcwspace = os.path.join('all.xcworkspace', 'contents.xcworkspacedata')
  CopyFileIfChanged(os.path.join(input_dir, xcwspace),
                    os.path.join(output_dir, xcwspace))

  # TODO(crbug.com/852522): Disable new BuildSystemType.
  DisableNewBuildSystem(output_dir)

  # TODO(crbug.com/679110): gn has been modified to remove 'sources.xcodeproj'
  # and keep 'all.xcworkspace' and 'products.xcodeproj'. The following code is
  # here to support both old and new projects setup and will be removed once gn
  # has rolled past it.
  sources = os.path.join('sources.xcodeproj', 'project.pbxproj')
  if os.path.isfile(os.path.join(input_dir, sources)):
    CopyFileIfChanged(os.path.join(input_dir, sources),
                      os.path.join(output_dir, sources))

def Main(args):
  parser = argparse.ArgumentParser(
      description='Convert GN Xcode projects for iOS.')
  parser.add_argument(
      'input',
      help='directory containing [product|all] Xcode projects.')
  parser.add_argument(
      'output',
      help='directory where to generate the iOS configuration.')
  parser.add_argument(
      '--add-config', dest='configurations', default=[], action='append',
      help='configuration to add to the Xcode project')
  parser.add_argument(
      '--root', type=os.path.abspath, required=True,
      help='root directory of the project')
  args = parser.parse_args(args)

  if not os.path.isdir(args.input):
    sys.stderr.write('Input directory does not exists.\n')
    return 1

  required = set(['products.xcodeproj', 'all.xcworkspace'])
  if not required.issubset(os.listdir(args.input)):
    sys.stderr.write(
        'Input directory does not contain all necessary Xcode projects.\n')
    return 1

  if not args.configurations:
    sys.stderr.write('At least one configuration required, see --add-config.\n')
    return 1

  ConvertGnXcodeProject(args.root, args.input, args.output, args.configurations)

if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

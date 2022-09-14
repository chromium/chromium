#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import re
import sys

if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)

import buildbot_common
import build_projects
import build_version
import easy_template
import parse_dsc

from build_paths import SDK_SRC_DIR, OUT_DIR, SDK_RESOURCE_DIR

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos
import oshelpers


def RemoveBuildCruft(outdir):
  for root, _, files in os.walk(outdir):
    for f in files:
      path = os.path.join(root, f)
      ext = os.path.splitext(path)[1]
      # Remove unwanted files from the package. Also remove manifest.json files
      # (which we usually want). These ones are the manifests of the invidual
      # examples, though, which CWS complains about. The master manifest.json
      # is generated after we call RemoveBuildCruft.
      if (ext in ('.d', '.o') or
          f == 'dir.stamp' or
          f == 'manifest.json' or
          re.search(r'_unstripped_.*?\.nexe', f)):
        buildbot_common.RemoveFile(path)


def StripNexes(outdir, platform, pepperdir):
  for root, _, files in os.walk(outdir):
    for f in files:
      path = os.path.join(root, f)
      m = re.search(r'lib(32|64).*\.so', path)
      arch = None
      if m:
        # System .so file. Must be x86, because ARM doesn't support glibc yet.
        arch = 'x86_' + m.group(1)
      else:
        basename, ext = os.path.splitext(f)
        if ext in ('.nexe', '.so'):
          # We can get the arch from the filename...
          valid_arches = ('x86_64', 'x86_32', 'arm')
          for a in valid_arches:
            if basename.endswith(a):
              arch = a
              break
      if not arch:
        continue

      strip = GetStrip(pepperdir, platform, arch, 'newlib')
      buildbot_common.Run([strip, path])


def GetStrip(pepperdir, platform, arch, toolchain):
  base_arch = {'x86_32': 'x86', 'x86_64': 'x86', 'arm': 'arm'}[arch]
  bin_dir = os.path.join(pepperdir, 'toolchain',
                         '%s_%s_%s' % (platform, base_arch, toolchain), 'bin')
  strip_prefix = {'x86_32': 'i686', 'x86_64': 'x86_64', 'arm': 'arm'}[arch]
  strip_name = '%s-nacl-strip' % strip_prefix
  return os.path.join(bin_dir, strip_name)


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('-c', '--channel',
      help='Channel to display in the name of the package.')

  # To setup bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete build_app.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(args)

  if options.channel:
    if options.channel not in ('Dev', 'Beta'):
      parser.error('Unknown channel: %s' % options.channel)

  toolchains = ['newlib', 'glibc']

  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)
  app_dir = os.path.join(OUT_DIR, 'naclsdk_app')
  app_examples_dir = os.path.join(app_dir, 'examples')
  sdk_resources_dir = SDK_RESOURCE_DIR
  platform = getos.GetPlatform()

  buildbot_common.RemoveDir(app_dir)
  buildbot_common.MakeDir(app_dir)

  # Add some dummy directories so build_projects doesn't complain...
  buildbot_common.MakeDir(os.path.join(app_dir, 'tools'))
  buildbot_common.MakeDir(os.path.join(app_dir, 'toolchain'))

  config = 'Release'

  filters = {}
  filters['DISABLE_PACKAGE'] = False
  filters['EXPERIMENTAL'] = False
  filters['TOOLS'] = toolchains
  filters['DEST'] = ['examples/api', 'examples/getting_started',
                     'examples/demo', 'examples/tutorial']
  tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=filters)
  build_projects.UpdateHelpers(app_dir, clobber=True)
  build_projects.UpdateProjects(app_dir, tree, clobber=False,
                                toolchains=toolchains, configs=[config],
                                first_toolchain=True)

  # Collect permissions from each example, and aggregate them.
  def MergeLists(list1, list2):
    return list1 + [x for x in list2 if x not in list1]
  all_permissions = []
  all_socket_permissions = []
  all_filesystem_permissions = []
  for _, project in parse_dsc.GenerateProjects(tree):
    permissions = project.get('PERMISSIONS', [])
    all_permissions = MergeLists(all_permissions, permissions)
    socket_permissions = project.get('SOCKET_PERMISSIONS', [])
    all_socket_permissions = MergeLists(all_socket_permissions,
                                        socket_permissions)
    filesystem_permissions = project.get('FILESYSTEM_PERMISSIONS', [])
    all_filesystem_permissions = MergeLists(all_filesystem_permissions,
                                            filesystem_permissions)
  if all_socket_permissions:
    all_permissions.append({'socket': all_socket_permissions})
  if all_filesystem_permissions:
    all_permissions.append({'fileSystem': all_filesystem_permissions})
  pretty_permissions = json.dumps(all_permissions, sort_keys=True, indent=4)

  for filename in ['background.js', 'icon128.png']:
    buildbot_common.CopyFile(os.path.join(sdk_resources_dir, filename),
                             os.path.join(app_examples_dir, filename))

  os.environ['NACL_SDK_ROOT'] = pepperdir

  build_projects.BuildProjects(app_dir, tree, deps=False, clean=False,
                               config=config)

  RemoveBuildCruft(app_dir)
  StripNexes(app_dir, platform, pepperdir)

  # Add manifest.json after RemoveBuildCruft... that function removes the
  # manifest.json files for the individual examples.
  name = 'Native Client SDK'
  if options.channel:
    name += ' (%s)' % options.channel
  template_dict = {
    'name': name,
    'channel': options.channel,
    'description':
        'Native Client SDK examples, showing API use and key concepts.',
    'key': False,  # manifests with "key" are rejected when uploading to CWS.
    'permissions': pretty_permissions,
    'version': build_version.ChromeVersionNoTrunk()
  }
  easy_template.RunTemplateFile(
      os.path.join(sdk_resources_dir, 'manifest.json.template'),
      os.path.join(app_examples_dir, 'manifest.json'),
      template_dict)

  app_zip = os.path.join(app_dir, 'examples.zip')
  os.chdir(app_examples_dir)
  oshelpers.Zip([app_zip, '-r', '*'])

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

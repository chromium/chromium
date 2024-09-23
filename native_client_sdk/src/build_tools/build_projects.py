#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import multiprocessing
import os
import posixpath
import sys
import urllib2

import buildbot_common
import build_version
import generate_make
import parse_dsc

from build_paths import SDK_SRC_DIR, OUT_DIR, SDK_RESOURCE_DIR
from build_paths import GSTORE
from generate_index import LandingPage

sys.path.append(os.path.join(SDK_SRC_DIR, 'tools'))
import getos


MAKE = 'nacl_sdk/make_3.99.90-26-gf80222c/make.exe'
LIB_DICT = {
  'linux': [],
  'mac': [],
  'win': ['x86_32']
}
VALID_TOOLCHAINS = [
  'clang-newlib',
  'glibc',
  'pnacl',
  'win',
  'linux',
  'mac',
]

# Global verbosity setting.
# If set to True (normally via a command line arg) then build_projects will
# add V=1 to all calls to 'make'
verbose = False


def Trace(msg):
  if verbose:
    sys.stderr.write(str(msg) + '\n')


def CopyFilesFromTo(filelist, srcdir, dstdir):
  for filename in filelist:
    srcpath = os.path.join(srcdir, filename)
    dstpath = os.path.join(dstdir, filename)
    buildbot_common.CopyFile(srcpath, dstpath)


def UpdateHelpers(pepperdir, clobber=False):
  tools_dir = os.path.join(pepperdir, 'tools')
  if not os.path.exists(tools_dir):
    buildbot_common.ErrorExit('SDK tools dir is missing: %s' % tools_dir)

  exampledir = os.path.join(pepperdir, 'examples')
  if clobber:
    buildbot_common.RemoveDir(exampledir)
  buildbot_common.MakeDir(exampledir)

  # Copy files for individual build and landing page
  files = ['favicon.ico', 'httpd.cmd', 'index.css', 'index.js',
      'button_close.png', 'button_close_hover.png']
  CopyFilesFromTo(files, SDK_RESOURCE_DIR, exampledir)

  # Copy tools scripts and make includes
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.py'),
      tools_dir)
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', '*.mk'),
      tools_dir)

  # Copy tools/lib scripts
  tools_lib_dir = os.path.join(pepperdir, 'tools', 'lib')
  buildbot_common.MakeDir(tools_lib_dir)
  buildbot_common.CopyDir(os.path.join(SDK_SRC_DIR, 'tools', 'lib', '*.py'),
      tools_lib_dir)

  # On Windows add a prebuilt make
  if getos.GetPlatform() == 'win':
    buildbot_common.BuildStep('Add MAKE')
    make_url = posixpath.join(GSTORE, MAKE)
    make_exe = os.path.join(tools_dir, 'make.exe')
    with open(make_exe, 'wb') as f:
      f.write(urllib2.urlopen(make_url).read())


def ValidateToolchains(toolchains):
  invalid_toolchains = set(toolchains) - set(VALID_TOOLCHAINS)
  if invalid_toolchains:
    buildbot_common.ErrorExit('Invalid toolchain(s): %s' % (
        ', '.join(invalid_toolchains)))


def GetDeps(projects):
  out = {}

  # Build list of all project names
  localtargets = [proj['NAME'] for proj in projects]

  # For each project
  for proj in projects:
    deplist = []
    # generate a list of dependencies
    for targ in proj.get('TARGETS', []):
      deplist.extend(targ.get('DEPS', []) + targ.get('LIBS', []))

    # and add dependencies to targets built in this subtree
    localdeps = [dep for dep in deplist if dep in localtargets]
    if localdeps:
      out[proj['NAME']] = localdeps

  return out


def UpdateProjects(pepperdir, project_tree, toolchains,
                   clobber=False, configs=None, first_toolchain=False):
  if configs is None:
    configs = ['Debug', 'Release']
  if not os.path.exists(os.path.join(pepperdir, 'tools')):
    buildbot_common.ErrorExit('Examples depend on missing tools.')
  if not os.path.exists(os.path.join(pepperdir, 'toolchain')):
    buildbot_common.ErrorExit('Examples depend on missing toolchains.')

  ValidateToolchains(toolchains)

  # Create the library output directories
  libdir = os.path.join(pepperdir, 'lib')
  platform = getos.GetPlatform()
  for config in configs:
    for arch in LIB_DICT[platform]:
      dirpath = os.path.join(libdir, '%s_%s_host' % (platform, arch), config)
      if clobber:
        buildbot_common.RemoveDir(dirpath)
      buildbot_common.MakeDir(dirpath)

  landing_page = None
  for branch, projects in project_tree.iteritems():
    dirpath = os.path.join(pepperdir, branch)
    if clobber:
      buildbot_common.RemoveDir(dirpath)
    buildbot_common.MakeDir(dirpath)
    targets = [desc['NAME'] for desc in projects if 'TARGETS' in desc]
    deps = GetDeps(projects)

    # Generate master make for this branch of projects
    generate_make.GenerateMasterMakefile(pepperdir,
                                         os.path.join(pepperdir, branch),
                                         targets, deps)

    if branch.startswith('examples') and not landing_page:
      landing_page = LandingPage()

    # Generate individual projects
    for desc in projects:
      srcroot = os.path.dirname(desc['FILEPATH'])
      generate_make.ProcessProject(pepperdir, srcroot, pepperdir, desc,
                                   toolchains, configs=configs,
                                   first_toolchain=first_toolchain)

      if branch.startswith('examples'):
        landing_page.AddDesc(desc)

  if landing_page:
    # Generate the landing page text file.
    index_html = os.path.join(pepperdir, 'examples', 'index.html')
    index_template = os.path.join(SDK_RESOURCE_DIR, 'index.html.template')
    with open(index_html, 'w') as fh:
      out = landing_page.GeneratePage(index_template)
      fh.write(out)

  # Generate top Make for examples
  targets = ['api', 'demo', 'getting_started', 'tutorial']
  targets = [x for x in targets if 'examples/'+x in project_tree]
  branch_name = 'examples'
  generate_make.GenerateMasterMakefile(pepperdir,
                                       os.path.join(pepperdir, branch_name),
                                       targets, {})


def BuildProjectsBranch(pepperdir, branch, deps, clean, config, args=None):
  make_dir = os.path.join(pepperdir, branch)
  print "\nMake: " + make_dir

  if getos.GetPlatform() == 'win':
    # We need to modify the environment to build host on Windows.
    make = os.path.join(make_dir, 'make.bat')
  else:
    make = 'make'

  env = None
  jobs = str(multiprocessing.cpu_count())

  make_cmd = [make, '-j', jobs]

  make_cmd.append('CONFIG='+config)
  if not deps:
    make_cmd.append('IGNORE_DEPS=1')

  if verbose:
    make_cmd.append('V=1')

  if args:
    make_cmd += args
  else:
    make_cmd.append('TOOLCHAIN=all')

  buildbot_common.Run(make_cmd, cwd=make_dir, env=env)
  if clean:
    # Clean to remove temporary files but keep the built
    buildbot_common.Run(make_cmd + ['clean'], cwd=make_dir, env=env)


def BuildProjects(pepperdir, project_tree, deps=True,
                  clean=False, config='Debug'):
  # Make sure we build libraries (which live in 'src') before
  # any of the examples.
  build_first = [p for p in project_tree if p != 'src']
  build_second = [p for p in project_tree if p == 'src']

  for branch in build_first + build_second:
    BuildProjectsBranch(pepperdir, branch, deps, clean, config)


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-c', '--clobber',
      help='Clobber project directories before copying new files',
      action='store_true', default=False)
  parser.add_argument('-b', '--build',
      help='Build the projects. Otherwise the projects are only copied.',
      action='store_true')
  parser.add_argument('--config',
      help='Choose configuration to build (Debug or Release).  Builds both '
           'by default')
  parser.add_argument('-x', '--experimental',
      help='Build experimental projects', action='store_true')
  parser.add_argument('-t', '--toolchain',
      help='Build using toolchain. Can be passed more than once.',
      action='append', default=[])
  parser.add_argument('-d', '--dest',
      help='Select which build destinations (project types) are valid.',
      action='append')
  parser.add_argument('projects', nargs='*',
      help='Select which projects to build.')
  parser.add_argument('-v', '--verbose', action='store_true')

  # To setup bash completion for this command first install optcomplete
  # and then add this line to your .bashrc:
  #  complete -F _optcomplete build_projects.py
  try:
    import optcomplete
    optcomplete.autocomplete(parser)
  except ImportError:
    pass

  options = parser.parse_args(args)

  global verbose
  if options.verbose:
    verbose = True

  buildbot_common.verbose = verbose

  if 'NACL_SDK_ROOT' in os.environ:
    # We don't want the currently configured NACL_SDK_ROOT to have any effect
    # on the build.
    del os.environ['NACL_SDK_ROOT']

  pepper_ver = str(int(build_version.ChromeMajorVersion()))
  pepperdir = os.path.join(OUT_DIR, 'pepper_' + pepper_ver)

  if not options.toolchain:
    # Order matters here: the default toolchain for an example's Makefile will
    # be the first toolchain in this list that is available in the example.
    # e.g. If an example supports clang-newlib and glibc, then the default will
    # be clang-newlib.
    options.toolchain = ['pnacl', 'clang-newlib', 'glibc', 'host']

  if 'host' in options.toolchain:
    options.toolchain.remove('host')
    options.toolchain.append(getos.GetPlatform())
    Trace('Adding platform: ' + getos.GetPlatform())

  ValidateToolchains(options.toolchain)

  filters = {}
  if options.toolchain:
    filters['TOOLS'] = options.toolchain
    Trace('Filter by toolchain: ' + str(options.toolchain))
  if not options.experimental:
    filters['EXPERIMENTAL'] = False
  if options.dest:
    filters['DEST'] = options.dest
    Trace('Filter by type: ' + str(options.dest))
  if options.projects:
    filters['NAME'] = options.projects
    Trace('Filter by name: ' + str(options.projects))

  try:
    project_tree = parse_dsc.LoadProjectTree(SDK_SRC_DIR, include=filters)
  except parse_dsc.ValidationError as e:
    buildbot_common.ErrorExit(str(e))

  if verbose:
    parse_dsc.PrintProjectTree(project_tree)

  UpdateHelpers(pepperdir, clobber=options.clobber)
  UpdateProjects(pepperdir, project_tree, options.toolchain,
                 clobber=options.clobber)

  if options.build:
    if options.config:
      configs = [options.config]
    else:
      configs = ['Debug', 'Release']
    for config in configs:
      BuildProjects(pepperdir, project_tree, config=config, deps=False)

  return 0


if __name__ == '__main__':
  script_name = os.path.basename(sys.argv[0])
  try:
    sys.exit(main(sys.argv[1:]))
  except parse_dsc.ValidationError as e:
    buildbot_common.ErrorExit('%s: %s' % (script_name, e))
  except KeyboardInterrupt:
    buildbot_common.ErrorExit('%s: interrupted' % script_name)

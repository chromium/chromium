#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import configparser
import convert_gn_xcodeproj
import errno
import io
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile


SUPPORTED_TARGETS = ('iphoneos', 'iphonesimulator', 'maccatalyst')
SUPPORTED_CONFIGS = ('Debug', 'Release', 'Profile', 'Official')
ADDITIONAL_FILE_ROOTS = ('//ios', '//ios_internal', '//docs', '//components')
ADDITIONAL_FILES_PATTERNS = ('*.md', '*_google_chrome_*.grd', 'OWNERS', 'DEPS')

# Pattern matching lines from ~/.lldbinit that must not be copied to the
# generated .lldbinit file. They match what the user were told to add to
# their global ~/.lldbinit file before setup-gn.py was updated to generate
# a project specific file and thus must not be copied as they would cause
# the settings to be overwritten.
LLDBINIT_SKIP_PATTERNS = (
    re.compile('^script sys.path\\[:0\\] = \\[\'.*/src/tools/lldb\'\\]$'),
    re.compile('^script import lldbinit$'),
    re.compile('^settings append target.source-map .* /google/src/.*$'),
)


def HostCpuArch():
  '''Returns the arch of the host cpu for GN.'''
  HOST_CPU_ARCH = {
    'arm64': '"arm64"',
    'x86_64': '"x64"',
  }
  return HOST_CPU_ARCH[platform.machine()]


class ConfigParserWithStringInterpolation(configparser.ConfigParser):

  '''A .ini file parser that supports strings and environment variables.'''

  ENV_VAR_PATTERN = re.compile(r'\$([A-Za-z0-9_]+)')

  def values(self, section):
    return filter(
        lambda val: val != '',
        map(lambda kv: self._UnquoteString(self._ExpandEnvVar(kv[1])),
            configparser.ConfigParser.items(self, section)))

  def getstring(self, section, option, fallback=''):
    try:
      raw_value = self.get(section, option)
    except configparser.NoOptionError:
      return fallback
    return self._UnquoteString(self._ExpandEnvVar(raw_value))

  def getboolean(self, section, option, fallback=False):
    try:
      return super().getboolean(section, option)
    except configparser.NoOptionError:
      return fallback

  def _UnquoteString(self, string):
    if not string or string[0] != '"' or string[-1] != '"':
      return string
    return string[1:-1]

  def _ExpandEnvVar(self, value):
    match = self.ENV_VAR_PATTERN.search(value)
    if not match:
      return value
    name, (begin, end) = match.group(1), match.span(0)
    prefix, suffix = value[:begin], self._ExpandEnvVar(value[end:])
    return prefix + os.environ.get(name, '') + suffix


class GnGenerator(object):

  '''Holds configuration for a build and method to generate gn default files.'''

  FAT_BUILD_DEFAULT_ARCH = '64-bit'

  TARGET_CPU_VALUES = {
    'iphoneos': '"arm64"',
    'iphonesimulator': HostCpuArch(),
    'maccatalyst': HostCpuArch(),
  }

  TARGET_ENVIRONMENT_VALUES = {
    'iphoneos': '"device"',
    'iphonesimulator': '"simulator"',
    'maccatalyst': '"catalyst"'
  }

  def __init__(self, settings, config, target):
    assert target in SUPPORTED_TARGETS
    assert config in SUPPORTED_CONFIGS
    self._settings = settings
    self._config = config
    self._target = target

  def _GetGnArgs(self):
    """Build the list of arguments to pass to gn.

    Returns:
      A list of tuple containing gn variable names and variable values (it
      is not a dictionary as the order needs to be preserved).
    """
    args = []

    is_debug = self._config == 'Debug'
    official = self._config == 'Official'
    is_optim = self._config in ('Profile', 'Official')

    args.append(('target_os', '"ios"'))
    args.append(('is_debug', is_debug))
    args.append(('enable_dsyms', is_optim))
    args.append(('enable_stripping', is_optim))
    args.append(('is_official_build', is_optim))
    args.append(('is_chrome_branded', official))

    if os.environ.get('FORCE_MAC_TOOLCHAIN', '0') == '1':
      args.append(('use_system_xcode', False))

    args.append(('target_cpu', self.TARGET_CPU_VALUES[self._target]))
    args.append((
        'target_environment',
        self.TARGET_ENVIRONMENT_VALUES[self._target]))

    # Add user overrides after the other configurations so that they can
    # refer to them and override them.
    args.extend(self._settings.items('gn_args'))
    return args


  def Generate(self, gn_path, proj_name, root_path, build_dir):
    self.WriteArgsGn(build_dir, xcode_project_name=proj_name)
    subprocess.check_call(self.GetGnCommand(
        gn_path, root_path, build_dir, xcode_project_name=proj_name))

  def CreateGnRules(self, gn_path, root_path, build_dir):
    gn_command = self.GetGnCommand(gn_path, root_path, build_dir)
    self.WriteArgsGn(build_dir)
    self.WriteBuildNinja(gn_command, build_dir)
    self.WriteBuildNinjaDeps(build_dir)

  def WriteArgsGn(self, build_dir, xcode_project_name=None):
    with open(os.path.join(build_dir, 'args.gn'), 'w') as stream:
      stream.write('# This file was generated by setup-gn.py. Do not edit\n')
      stream.write('# but instead use ~/.setup-gn or $repo/.setup-gn files\n')
      stream.write('# to configure settings.\n')
      stream.write('\n')

      if self._target != 'maccatalyst':
        if self._settings.has_section('$imports$'):
          for import_rule in self._settings.values('$imports$'):
            stream.write('import("%s")\n' % import_rule)
          stream.write('\n')

      gn_args = self._GetGnArgs()

      for name, value in gn_args:
        if isinstance(value, bool):
          stream.write('%s = %s\n' % (name, str(value).lower()))
        elif isinstance(value, list):
          stream.write('%s = [%s' % (name, '\n' if len(value) > 1 else ''))
          if len(value) == 1:
            prefix = ' '
            suffix = ' '
          else:
            prefix = '  '
            suffix = ',\n'
          for item in value:
            if isinstance(item, bool):
              stream.write('%s%s%s' % (prefix, str(item).lower(), suffix))
            else:
              stream.write('%s%s%s' % (prefix, item, suffix))
          stream.write(']\n')
        else:
          # ConfigParser removes quote around empty string which confuse
          # `gn gen` so restore them.
          if not value:
            value = '""'
          stream.write('%s = %s\n' % (name, value))

  def WriteBuildNinja(self, gn_command, build_dir):
    with open(os.path.join(build_dir, 'build.ninja'), 'w') as stream:
      stream.write('ninja_required_version = 1.7.2\n')
      stream.write('\n')
      stream.write('rule gn\n')
      stream.write('  command = %s\n' % NinjaEscapeCommand(gn_command))
      stream.write('  description = Regenerating ninja files\n')
      stream.write('\n')
      stream.write('build build.ninja.stamp: gn\n')
      stream.write('  generator = 1\n')
      stream.write('  depfile = build.ninja.d\n')
      stream.write('\n')
      stream.write('build build.ninja: phony build.ninja.stamp\n')
      stream.write('  generator = 1\n')
      stream.write('\n')

  def WriteBuildNinjaDeps(self, build_dir):
    with open(os.path.join(build_dir, 'build.ninja.d'), 'w') as stream:
      stream.write('build.ninja: nonexistant_file.gn\n')

  def GetGnCommand(self, gn_path, src_path, out_path, xcode_project_name=None):
    gn_command = [ gn_path, '--root=%s' % os.path.realpath(src_path), '-q' ]
    if xcode_project_name is not None:
      gn_command.append('--ide=xcode')
      gn_command.append('--ninja-executable=autoninja')
      gn_command.append('--xcode-build-system=new')
      gn_command.append('--xcode-project=%s' % xcode_project_name)
      gn_command.append('--xcode-additional-files-patterns=' +
                        ';'.join(ADDITIONAL_FILES_PATTERNS))
      gn_command.append(
          '--xcode-additional-files-roots=' + ';'.join(ADDITIONAL_FILE_ROOTS))
      gn_command.append('--xcode-configs=' + ';'.join(SUPPORTED_CONFIGS))
      gn_command.append('--xcode-config-build-dir='
                        '//out/${CONFIGURATION}${EFFECTIVE_PLATFORM_NAME}')
      use_blink = self._settings.getboolean('gn_args', 'use_blink')
      if self._settings.has_section('filters') and not use_blink:
        target_filters = self._settings.values('filters')
        if target_filters:
          gn_command.append('--filters=%s' % ';'.join(target_filters))
    else:
      gn_command.append('--check')
    gn_command.append('gen')
    gn_command.append('//%s' %
        os.path.relpath(os.path.abspath(out_path), os.path.abspath(src_path)))
    return gn_command


def NinjaNeedEscape(arg):
  '''Returns True if |arg| needs to be escaped when written to .ninja file.'''
  return ':' in arg or '*' in arg or ';' in arg


def NinjaEscapeCommand(command):
  '''Escapes |command| in order to write it to .ninja file.'''
  result = []
  for arg in command:
    if NinjaNeedEscape(arg):
      arg = arg.replace(':', '$:')
      arg = arg.replace(';', '\\;')
      arg = arg.replace('*', '\\*')
    else:
      result.append(arg)
  return ' '.join(result)


def FindGn():
  '''Returns absolute path to gn binary looking at the PATH env variable.'''
  for path in os.environ['PATH'].split(os.path.pathsep):
    gn_path = os.path.join(path, 'gn')
    if os.path.isfile(gn_path) and os.access(gn_path, os.X_OK):
      return gn_path
  return None


def GenerateXcodeProject(gn_path, root_dir, proj_name, out_dir, settings):
  '''Generate Xcode project with Xcode and convert to multi-configurations.'''
  prefix = os.path.abspath(os.path.join(out_dir, '_temp'))
  temp_path = tempfile.mkdtemp(prefix=prefix)
  try:
    generator = GnGenerator(settings, 'Debug', 'iphonesimulator')
    generator.Generate(gn_path, proj_name, root_dir, temp_path)
    convert_gn_xcodeproj.ConvertGnXcodeProject(
        root_dir,
        '%s.xcodeproj' % proj_name,
        os.path.join(temp_path),
        os.path.join(out_dir, 'build'),
        SUPPORTED_CONFIGS)
  finally:
    if os.path.exists(temp_path):
      shutil.rmtree(temp_path)

def CreateLLDBInitFile(root_dir, out_dir, settings):
  '''
  Generate an .lldbinit file for the project that load the script that fixes
  the mapping of source files (see docs/ios/build_instructions.md#debugging).
  '''
  with open(os.path.join(out_dir, 'build', '.lldbinit'), 'w') as lldbinit:
    lldb_script_dir = os.path.join(os.path.abspath(root_dir), 'tools', 'lldb')
    lldbinit.write('script sys.path[:0] = [\'%s\']\n' % lldb_script_dir)
    lldbinit.write('script import lldbinit\n')

    workspace_name = settings.getstring(
        'gn_args',
        'ios_internal_citc_workspace_name')

    if workspace_name != '':
      username = os.environ['USER']
      for shortname in ('googlemac', 'third_party', 'blaze-out'):
        lldbinit.write('settings append target.source-map %s %s\n' % (
            shortname,
            '/google/src/cloud/%s/%s/google3/%s' % (
                username, workspace_name, shortname)))

    # Append the content of //ios/build/tools/lldbinit.defaults if it exists.
    tools_dir = os.path.join(root_dir, 'ios', 'build', 'tools')
    defaults_lldbinit_path = os.path.join(tools_dir, 'lldbinit.defaults')
    if os.path.isfile(defaults_lldbinit_path):
      with open(defaults_lldbinit_path) as defaults_lldbinit:
        for line in defaults_lldbinit:
          lldbinit.write(line)

    # Append the content of ~/.lldbinit if it exists. Line that look like they
    # are trying to configure source mapping are skipped as they probably date
    # back from when setup-gn.py was not generating an .lldbinit file.
    global_lldbinit_path = os.path.join(os.environ['HOME'], '.lldbinit')
    if os.path.isfile(global_lldbinit_path):
      with open(global_lldbinit_path) as global_lldbinit:
        for line in global_lldbinit:
          if any(pattern.match(line) for pattern in LLDBINIT_SKIP_PATTERNS):
            continue
          lldbinit.write(line)


def GenerateGnBuildRules(gn_path, root_dir, out_dir, settings):
  '''Generates all template configurations for gn.'''
  for config in SUPPORTED_CONFIGS:
    for target in SUPPORTED_TARGETS:
      build_dir = os.path.join(out_dir, '%s-%s' % (config, target))
      if not os.path.isdir(build_dir):
        os.makedirs(build_dir)

      generator = GnGenerator(settings, config, target)
      generator.CreateGnRules(gn_path, root_dir, build_dir)


def Main(args):
  default_root = os.path.normpath(os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))

  parser = argparse.ArgumentParser(
      description='Generate build directories for use with gn.')
  parser.add_argument(
      'root', default=default_root, nargs='?',
      help='root directory where to generate multiple out configurations')
  parser.add_argument(
      '--import', action='append', dest='import_rules', default=[],
      help='path to file defining default gn variables')
  parser.add_argument(
      '--gn-path', default=None,
      help='path to gn binary (default: look up in $PATH)')
  parser.add_argument(
      '--build-dir', default='out',
      help='path where the build should be created (default: %(default)s)')
  parser.add_argument(
      '--config-path', default=os.path.expanduser('~/.setup-gn'),
      help='path to the user config file (default: %(default)s)')
  parser.add_argument(
      '--project-config-path', default=os.path.join(default_root, os.pardir,
          '.setup-gn'),
      help='path to the project config file (default: %(default)s)')
  parser.add_argument(
      '--system-config-path', default=os.path.splitext(__file__)[0] + '.config',
      help='path to the default config file (default: %(default)s)')
  parser.add_argument(
      '--project-name', default='all', dest='proj_name',
      help='name of the generated Xcode project (default: %(default)s)')
  parser.add_argument(
      '--no-xcode-project', action='store_true', default=False,
      help='do not generate the build directory with XCode project')
  args = parser.parse_args(args)

  # Load configuration (first global and then any user overrides).
  settings = ConfigParserWithStringInterpolation()
  settings.read([
      args.system_config_path,
      args.config_path,
      args.project_config_path,
  ])

  # Add private sections corresponding to --import argument.
  if args.import_rules:
    settings.add_section('$imports$')
    for i, import_rule in enumerate(args.import_rules):
      if not import_rule.startswith('//'):
        import_rule = '//%s' % os.path.relpath(
            os.path.abspath(import_rule), os.path.abspath(args.root))
      settings.set('$imports$', '$rule%d$' % i, import_rule)

  # Validate settings.
  if settings.getstring('build', 'arch') not in ('64-bit', '32-bit', 'fat'):
    sys.stderr.write('ERROR: invalid value for build.arch: %s\n' %
        settings.getstring('build', 'arch'))
    sys.exit(1)

  # Find path to gn binary either from command-line or in PATH.
  if args.gn_path:
    gn_path = args.gn_path
  else:
    gn_path = FindGn()
    if gn_path is None:
      sys.stderr.write('ERROR: cannot find gn in PATH\n')
      sys.exit(1)

  out_dir = os.path.join(args.root, args.build_dir)
  if not os.path.isdir(out_dir):
    os.makedirs(out_dir)

  if not args.no_xcode_project:
    GenerateXcodeProject(gn_path, args.root, args.proj_name, out_dir, settings)
    CreateLLDBInitFile(args.root, out_dir, settings)
  GenerateGnBuildRules(gn_path, args.root, out_dir, settings)


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))

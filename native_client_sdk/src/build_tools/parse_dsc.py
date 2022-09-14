#!/usr/bin/env python
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import fnmatch
import io
import os
import sys

VALID_TOOLCHAINS = [
  'clang-newlib',
  'glibc',
  'pnacl',
  'win',
  'linux',
  'mac',
]

# 'KEY' : ( <TYPE>, [Accepted Values], <Required?>)
DSC_FORMAT = {
    'DISABLE': (bool, [True, False], False),
    'SEL_LDR': (bool, [True, False], False),
    # Disable this project from being included in the NaCl packaged app.
    'DISABLE_PACKAGE': (bool, [True, False], False),
    # Don't generate the additional files to allow this project to run as a
    # packaged app (i.e. manifest.json, background.js, etc.).
    'NO_PACKAGE_FILES': (bool, [True, False], False),
    'TOOLS' : (list, VALID_TOOLCHAINS, False),
    'CONFIGS' : (list, ['Debug', 'Release'], False),
    'PREREQ' : (list, '', False),
    'TARGETS' : (list, {
        'NAME': (str, '', True),
        # main = nexe target
        # lib = library target
        # so = shared object target, automatically added to NMF
        # so-standalone =  shared object target, not put into NMF
        'TYPE': (str,
                 ['main', 'lib', 'static-lib', 'so', 'so-standalone',
                  'linker-script'],
                 True),
        'SOURCES': (list, '', True),
        'EXTRA_SOURCES': (list, '', False),
        'CFLAGS': (list, '', False),
        'CFLAGS_GCC': (list, '', False),
        'CXXFLAGS': (list, '', False),
        'DEFINES': (list, '', False),
        'LDFLAGS': (list, '', False),
        'INCLUDES': (list, '', False),
        'LIBS' : (dict, VALID_TOOLCHAINS, False),
        'DEPS' : (list, '', False)
    }, False),
    'HEADERS': (list, {
        'FILES': (list, '', True),
        'DEST': (str, '', True),
    }, False),
    'SEARCH': (list, '', False),
    'POST': (str, '', False),
    'PRE': (str, '', False),
    'DEST': (str, ['getting_started', 'examples/api',
                   'examples/demo', 'examples/tutorial',
                   'src', 'tests'], True),
    'NAME': (str, '', False),
    'DATA': (list, '', False),
    'TITLE': (str, '', False),
    'GROUP': (str, '', False),
    'EXPERIMENTAL': (bool, [True, False], False),
    'PERMISSIONS': (list, '', False),
    'SOCKET_PERMISSIONS': (list, '', False),
    'FILESYSTEM_PERMISSIONS': (list, '', False),
    'MULTI_PLATFORM': (bool, [True, False], False),
    'MIN_CHROME_VERSION': (str, '', False),
}


class ValidationError(Exception):
  pass


def ValidateFormat(src, dsc_format):
  # Verify all required keys are there
  for key in dsc_format:
    exp_type, exp_value, required = dsc_format[key]
    if required and key not in src:
      raise ValidationError('Missing required key %s.' % key)

  # For each provided key, verify it's valid
  for key in src:
    # Verify the key is known
    if key not in dsc_format:
      raise ValidationError('Unexpected key %s.' % key)

    exp_type, exp_value, required = dsc_format[key]
    value = src[key]

    # Verify the value is non-empty if required
    if required and not value:
      raise ValidationError('Expected non-empty value for %s.' % key)

    # If the expected type is a dict, but the provided type is a list
    # then the list applies to all keys of the dictionary, so we reset
    # the expected type and value.
    if exp_type is dict:
      if isinstance(value, list):
        exp_type = list
        exp_value = ''

    # Verify the key is of the expected type
    if not isinstance(value, exp_type):
      raise ValidationError('Key %s expects %s not %s.' % (
          key, exp_type.__name__.upper(), type(value).__name__.upper()))

    # If it's a bool, the expected values are always True or False.
    if exp_type is bool:
      continue

    # If it's a string and there are expected values, make sure it matches
    if exp_type is str:
      if isinstance(exp_value, list) and exp_value:
        if value not in exp_value:
          raise ValidationError("Value '%s' not expected for %s." %
                                (value, key))
      continue

    # if it's a list, then we need to validate the values
    if exp_type is list:
      # If we expect a dictionary, then call this recursively
      if isinstance(exp_value, dict):
        for val in value:
          ValidateFormat(val, exp_value)
        continue
      # If we expect a list of strings
      if isinstance(exp_value, str):
        for val in value:
          if not isinstance(val, str):
            raise ValidationError('Value %s in %s is not a string.' %
                                  (val, key))
        continue
      # if we expect a particular string
      if isinstance(exp_value, list):
        for val in value:
          if val not in exp_value:
            raise ValidationError('Value %s not expected in %s.' %
                                  (val, key))
        continue

    # if we are expecting a dict, verify the keys are allowed
    if exp_type is dict:
      print('Expecting dict\n')
      for sub in value:
        if sub not in exp_value:
          raise ValidationError('Sub key %s not expected in %s.' %
                                (sub, key))
      continue

    # If we got this far, it's an unexpected type
    raise ValidationError('Unexpected type %s for key %s.' %
                          (str(type(src[key])), key))


def LoadProject(filename):
  with io.open(filename, encoding='utf-8') as descfile:
    try:
      desc = eval(descfile.read(), {}, {})
    except Exception as e:
      raise ValidationError(e)
  if desc.get('DISABLE', False):
    return None
  ValidateFormat(desc, DSC_FORMAT)
  desc['FILEPATH'] = os.path.abspath(filename)
  desc.setdefault('TOOLS', VALID_TOOLCHAINS)
  return desc


def LoadProjectTreeUnfiltered(srcpath):
  # Build the tree
  out = collections.defaultdict(list)
  for root, _, files in os.walk(srcpath):
    for filename in files:
      if fnmatch.fnmatch(filename, '*.dsc'):
        filepath = os.path.join(root, filename)
        try:
          desc = LoadProject(filepath)
        except ValidationError as e:
          raise ValidationError("Failed to validate: %s: %s" % (filepath, e))
        if desc:
          key = desc['DEST']
          out[key].append(desc)
  return out


def LoadProjectTree(srcpath, include, exclude=None):
  out = LoadProjectTreeUnfiltered(srcpath)
  return FilterTree(out, MakeDefaultFilterFn(include, exclude))


def GenerateProjects(tree):
  for key in tree:
    for val in tree[key]:
      yield key, val


def FilterTree(tree, filter_fn):
  out = collections.defaultdict(list)
  for branch, desc in GenerateProjects(tree):
    if filter_fn(desc):
      out[branch].append(desc)
  return out


def MakeDefaultFilterFn(include, exclude):
  def DefaultFilterFn(desc):
    matches_include = not include or DescMatchesFilter(desc, include)
    matches_exclude = exclude and DescMatchesFilter(desc, exclude)

    # Exclude list overrides include list.
    if matches_exclude:
      return False
    return matches_include

  return DefaultFilterFn


def DescMatchesFilter(desc, filters):
  for key, expected in iter(filters.items()):
    # For any filtered key which is unspecified, assumed False
    value = desc.get(key, False)

    # If we provide an expected list, match at least one
    if not isinstance(expected, (list, tuple)):
      expected = set([expected])
    if not isinstance(value, list):
      value = set([value])

    if not set(expected) & set(value):
      return False

  # If we fall through, then we matched the filters
  return True


def PrintProjectTree(tree):
  for key in tree:
    print(key + ':')
    for val in tree[key]:
      print('\t' + val['NAME'])


def main(args):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-e', '--experimental',
      help='build experimental examples and libraries', action='store_true')
  parser.add_argument('-t', '--toolchain',
      help='Build using toolchain. Can be passed more than once.',
      action='append')
  parser.add_argument('project_root', default='.')

  options = parser.parse_args(args)
  filters = {}

  if options.toolchain:
    filters['TOOLS'] = options.toolchain

  if not options.experimental:
    filters['EXPERIMENTAL'] = False

  try:
    tree = LoadProjectTree(options.project_root, include=filters)
  except ValidationError as e:
    sys.stderr.write(str(e) + '\n')
    return 1

  PrintProjectTree(tree)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

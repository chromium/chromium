#!/usr/bin/env python
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to regenerate API docs using doxygen.
"""

import argparse
import collections
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib2


if sys.version_info < (2, 7, 0):
  sys.stderr.write("python 2.7 or later is required run this script\n")
  sys.exit(1)


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DOC_DIR = os.path.dirname(SCRIPT_DIR)


ChannelInfo = collections.namedtuple('ChannelInfo', ['branch', 'version'])


def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')

Trace.verbose = False


def GetChannelInfo():
  url = 'http://omahaproxy.appspot.com/json'
  u = urllib2.urlopen(url)
  try:
    data = json.loads(u.read())
  finally:
    u.close()

  channel_info = {}
  for os_row in data:
    osname = os_row['os']
    if osname not in ('win', 'mac', 'linux'):
      continue
    for version_row in os_row['versions']:
      channel = version_row['channel']
      # We don't display canary docs.
      if channel.startswith('canary'):
        continue

      version = version_row['version'].split('.')[0]  # Major version
      branch = version_row['true_branch']
      if branch is None:
        branch = 'trunk'

      if channel in channel_info:
        existing_info = channel_info[channel]
        if branch != existing_info.branch:
          sys.stderr.write('Warning: found different branch numbers for '
              'channel %s: %s vs %s. Using %s.\n' % (
              channel, branch, existing_info.branch, existing_info.branch))
      else:
        channel_info[channel] = ChannelInfo(branch, version)

  return channel_info


def RemoveFile(filename):
  if os.path.exists(filename):
    os.remove(filename)


def RemoveDir(dirname):
  if os.path.exists(dirname):
    shutil.rmtree(dirname)


def HasBranchHeads():
  cmd = ['git', 'for-each-ref', '--format=%(refname)',
         'refs/remotes/branch-heads']
  output = subprocess.check_output(cmd).splitlines()
  return output != []


def CheckoutDirectories(dest_dirname, refname, root_path, patterns=None):
  treeish = '%s:%s' % (refname, root_path)
  cmd = ['git', 'ls-tree', '--full-tree', '-r', treeish]
  if patterns:
    cmd.extend(patterns)

  Trace('Running \"%s\":' % ' '.join(cmd))
  output = subprocess.check_output(cmd)
  for line in output.splitlines():
    info, rel_filename = line.split('\t')
    sha = info.split(' ')[2]

    Trace('  %s %s' % (sha, rel_filename))

    cmd = ['git', 'show', sha]
    blob = subprocess.check_output(cmd)
    filename = os.path.join(dest_dirname, rel_filename)
    dirname = os.path.dirname(filename)
    if not os.path.exists(dirname):
      os.makedirs(dirname)

    Trace('    writing to %s' % filename)
    with open(filename, 'w') as f:
      f.write(blob)


def CheckoutPepperDocs(branch, doc_dirname):
  Trace('Removing directory %s' % doc_dirname)
  RemoveDir(doc_dirname)

  if branch == 'master':
    refname = 'refs/remotes/origin/master'
  else:
    refname = 'refs/remotes/branch-heads/%s' % branch

  Trace('Checking out docs into %s' % doc_dirname)
  subdirs = ['api', 'generators', 'cpp', 'utility']
  CheckoutDirectories(doc_dirname, refname, 'ppapi', subdirs)

  # The IDL generator needs PLY (a python lexing library); check it out into
  # generators.
  ply_dirname = os.path.join(doc_dirname, 'generators', 'ply')
  Trace('Checking out PLY into %s' % ply_dirname)
  CheckoutDirectories(ply_dirname, refname, 'third_party/ply')


def FixPepperDocLinks(doc_dirname):
  # TODO(binji): We can remove this step when the correct links are in the
  # stable branch.
  Trace('Looking for links to fix in Pepper headers...')
  for root, dirs, filenames in os.walk(doc_dirname):
    # Don't recurse into .svn
    if '.svn' in dirs:
      dirs.remove('.svn')

    for filename in filenames:
      header_filename = os.path.join(root, filename)
      Trace('  Checking file %r...' % header_filename)
      replacements = {
        '<a href="/native-client/{{pepperversion}}/devguide/coding/audio">':
            '<a href="/native-client/devguide/coding/audio.html">',
        '<a href="/native-client/devguide/coding/audio">':
            '<a href="/native-client/devguide/coding/audio.html">',
        '<a href="/native-client/{{pepperversion}}/pepperc/globals_defs"':
            '<a href="globals_defs.html"',
        '<a href="../pepperc/ppb__image__data_8h.html">':
            '<a href="../c/ppb__image__data_8h.html">'}

      with open(header_filename) as f:
        lines = []
        replaced = False
        for line in f:
          for find, replace in replacements.iteritems():
            pos = line.find(find)
            if pos != -1:
              Trace('    Found %r...' % find)
              replaced = True
              line = line[:pos] + replace + line[pos + len(find):]
          lines.append(line)

      if replaced:
        Trace('  Writing new file.')
        with open(header_filename, 'w') as f:
          f.writelines(lines)


def GenerateCHeaders(pepper_version, doc_dirname):
  script = os.path.join(os.pardir, 'generators', 'generator.py')
  cwd = os.path.join(doc_dirname, 'api')
  out_dirname = os.path.join(os.pardir, 'c')
  cmd = [sys.executable, script, '--cgen', '--release', 'M' + pepper_version,
         '--wnone', '--dstroot', out_dirname]
  Trace('Generating C Headers for version %s\n  %s' % (
      pepper_version, ' '.join(cmd)))
  subprocess.check_call(cmd, cwd=cwd)


def GenerateDoxyfile(template_filename, out_dirname, doc_dirname, doxyfile):
  Trace('Writing Doxyfile "%s" (from template %s)' % (
    doxyfile, template_filename))

  with open(template_filename) as f:
    data = f.read()

  with open(doxyfile, 'w') as f:
    f.write(data % {
      'out_dirname': out_dirname,
      'doc_dirname': doc_dirname,
      'script_dirname': SCRIPT_DIR})


def CheckDoxygenVersion(doxygen):
  version = subprocess.check_output([doxygen, '--version']).strip()
  url = 'http://ftp.stack.nl/pub/users/dimitri/doxygen-1.7.6.1.linux.bin.tar.gz'
  if version != '1.7.6.1':
    print 'Doxygen version 1.7.6.1 is required'
    print 'The version being used (%s) is version %s' % (doxygen, version)
    print 'The simplest way to grab this version is to download it directly:'
    print url
    print 'Then either add it to your $PATH or set $DOXYGEN to point to binary.'
    sys.exit(1)


def RunDoxygen(out_dirname, doxyfile):
  Trace('Removing old output directory %s' % out_dirname)
  RemoveDir(out_dirname)

  Trace('Making new output directory %s' % out_dirname)
  os.makedirs(out_dirname)

  doxygen = os.environ.get('DOXYGEN', 'doxygen')
  CheckDoxygenVersion(doxygen)
  cmd = [doxygen, doxyfile]
  Trace('Running Doxygen:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def RunDoxyCleanup(out_dirname):
  script = os.path.join(SCRIPT_DIR, 'doxy_cleanup.py')
  cmd = [sys.executable, script, out_dirname]
  if Trace.verbose:
    cmd.append('-v')
  Trace('Running doxy_cleanup:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def RunRstIndex(kind, channel, pepper_version, out_dirname, out_rst_filename):
  assert kind in ('root', 'c', 'cpp')
  script = os.path.join(SCRIPT_DIR, 'rst_index.py')
  cmd = [sys.executable, script,
         '--' + kind,
         '--channel', channel,
         '--version', pepper_version,
         out_dirname,
         out_rst_filename]
  Trace('Running rst_index:\n  %s' % ' '.join(cmd))
  subprocess.check_call(cmd)


def GetRstName(kind, channel):
  if channel == 'stable':
    filename = '%s-api.rst' % kind
  else:
    filename = '%s-api-%s.rst' % (kind, channel)
  return os.path.join(DOC_DIR, filename)


def GenerateDocs(root_dirname, channel, pepper_version, branch):
  Trace('Generating docs for %s (branch %s)' % (channel, branch))
  pepper_dirname = 'pepper_%s' % channel
  out_dirname = os.path.join(root_dirname, pepper_dirname)

  try:
    svn_dirname = tempfile.mkdtemp(prefix=pepper_dirname)
    doxyfile_dirname = tempfile.mkdtemp(prefix='%s_doxyfiles' % pepper_dirname)

    CheckoutPepperDocs(branch, svn_dirname)
    FixPepperDocLinks(svn_dirname)
    GenerateCHeaders(pepper_version, svn_dirname)

    doxyfile_c = ''
    doxyfile_cpp = ''

    # Generate Root index
    rst_index_root = os.path.join(DOC_DIR, pepper_dirname, 'index.rst')
    RunRstIndex('root', channel, pepper_version, out_dirname, rst_index_root)

    # Generate C docs
    out_dirname_c = os.path.join(out_dirname, 'c')
    doxyfile_c = os.path.join(doxyfile_dirname, 'Doxyfile.c.%s' % channel)
    doxyfile_c_template = os.path.join(SCRIPT_DIR, 'Doxyfile.c.template')
    rst_index_c = GetRstName('c', channel)
    GenerateDoxyfile(doxyfile_c_template, out_dirname_c, svn_dirname,
                     doxyfile_c)
    RunDoxygen(out_dirname_c, doxyfile_c)
    RunDoxyCleanup(out_dirname_c)
    RunRstIndex('c', channel, pepper_version, out_dirname_c, rst_index_c)

    # Generate C++ docs
    out_dirname_cpp = os.path.join(out_dirname, 'cpp')
    doxyfile_cpp = os.path.join(doxyfile_dirname, 'Doxyfile.cpp.%s' % channel)
    doxyfile_cpp_template = os.path.join(SCRIPT_DIR, 'Doxyfile.cpp.template')
    rst_index_cpp = GetRstName('cpp', channel)
    GenerateDoxyfile(doxyfile_cpp_template, out_dirname_cpp, svn_dirname,
                     doxyfile_cpp)
    RunDoxygen(out_dirname_cpp, doxyfile_cpp)
    RunDoxyCleanup(out_dirname_cpp)
    RunRstIndex('cpp', channel, pepper_version, out_dirname_cpp, rst_index_cpp)
  finally:
    # Cleanup
    RemoveDir(svn_dirname)
    RemoveDir(doxyfile_dirname)


def main(argv):
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-v', '--verbose',
                      help='Verbose output', action='store_true')
  parser.add_argument('out_directory')
  options = parser.parse_args(argv)

  if options.verbose:
    Trace.verbose = True

  for channel, info in GetChannelInfo().iteritems():
    GenerateDocs(options.out_directory, channel, info.version, info.branch)

  return 0


if __name__ == '__main__':
  try:
    rtn = main(sys.argv[1:])
  except KeyboardInterrupt:
    sys.stderr.write('%s: interrupted\n' % os.path.basename(__file__))
    rtn = 1
  sys.exit(rtn)

#!/usr/bin/env python3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a zip archive for the Chrome Remote Desktop Host installer.

This script builds a zip file that contains all the files needed to build an
installer for Chrome Remote Desktop Host.

This zip archive is then used by the signing bots to:
(1) Sign the binaries
(2) Build the final installer

TODO(garykac) We should consider merging this with build-webapp.py.
"""

import os
import shutil
import subprocess
import sys

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir,
    "build"))
import zip_helpers


def cleanDir(dir):
  """Deletes and recreates the dir to make sure it is clean.

  Args:
    dir: The directory to clean.
  """
  try:
    shutil.rmtree(dir)
  except OSError:
    if os.path.exists(dir):
      raise
    else:
      pass
  os.makedirs(dir, 0o775)


def buildDefDictionary(definitions):
  """Builds the definition dictionary from the VARIABLE=value array.

  Args:
    defs: Array of variable definitions: 'VARIABLE=value'.

    Returns:
      Dictionary with the definitions.
  """
  defs = {}
  for d in definitions:
    (key, val) = d.split('=')
    defs[key] = val
  return defs


def remapSrcFile(dst_root, src_roots, src_file):
  """Calculates destination file path and creates directory.

  Any matching |src_roots| prefix is stripped from |src_file| before
  appending to |dst_root|.

  For example, given:
    dst_root = '/output'
    src_roots = ['host/installer/mac']
    src_file = 'host/installer/mac/Scripts/keystone_install.sh'
  The final calculated path is:
    '/output/Scripts/keystone_install.sh'

  The |src_file| must match one of the |src_roots| prefixes. If there are no
  matches, then an error is reported.

  If multiple |src_roots| match, then only the first match is applied. Because
  of this, if you have roots that share a common prefix, the longest string
  should be first in this array.

  Args:
    dst_root: Target directory where files are copied.
    src_roots: Array of path prefixes which will be stripped of |src_file|
               (if they match) before appending it to the |dst_root|.
    src_file: Source file to be copied.
  Returns:
    Full path to destination file in |dst_root|.
  """
  # Strip of directory prefix.
  found_root = False
  for root in src_roots:
    root = os.path.normpath(root)
    src_file = os.path.normpath(src_file)
    if os.path.commonprefix([root, src_file]) == root:
      src_file = os.path.relpath(src_file, root)
      found_root = True
      break

  if not found_root:
    error('Unable to match prefix for %s' % src_file)

  dst_file = os.path.join(dst_root, src_file)
  # Make sure target directory exists.
  dst_dir = os.path.dirname(dst_file)
  if not os.path.exists(dst_dir):
    os.makedirs(dst_dir, 0o775)
  return dst_file


def copyFileWithDefs(src_file, dst_file, defs):
  """Copies from src_file to dst_file, performing variable substitution.

  Any @@VARIABLE@@ in the source is replaced with the value of VARIABLE
  in the |defs| dictionary when written to the destination file.

  Args:
    src_file: Full or relative path to source file to copy.
    dst_file: Relative path (and filename) where src_file should be copied.
    defs: Dictionary of variable definitions.
  """
  data = open(src_file, 'r').read()
  for key, val in defs.items():
    try:
      data = data.replace('@@' + key + '@@', val)
    except TypeError:
      print(repr(key), repr(val))
  open(dst_file, 'w').write(data)
  shutil.copystat(src_file, dst_file)


def copyZipIntoArchive(out_dir, files_root, zip_file):
  """Expands the zip_file into the out_dir, preserving the directory structure.

  Args:
    out_dir: Target directory where unzipped files are copied.
    files_root: Path prefix which is stripped of zip_file before appending
                it to the out_dir.
    zip_file: Relative path (and filename) to the zip file.
  """
  base_zip_name = os.path.basename(zip_file)

  # We don't use the 'zipfile' module here because it doesn't restore all the
  # file permissions correctly. We use the 'unzip' command manually.
  old_dir = os.getcwd();
  os.chdir(os.path.dirname(zip_file))
  subprocess.call(['unzip', '-qq', '-o', base_zip_name])
  os.chdir(old_dir)

  # Unzip into correct dir in out_dir.
  out_zip_path = remapSrcFile(out_dir, files_root, zip_file)
  out_zip_dir = os.path.dirname(out_zip_path)

  (src_dir, ignore1) = os.path.splitext(zip_file)
  (base_dir_name, ignore2) = os.path.splitext(base_zip_name)
  shutil.copytree(src_dir, os.path.join(out_zip_dir, base_dir_name))


def buildHostArchive(temp_dir, zip_path, source_file_roots, source_files,
                     gen_files, gen_files_dst, defs):
  """Builds a zip archive with the files needed to build the installer.

  Args:
    temp_dir: Temporary dir used to build up the contents for the archive.
    zip_path: Full path to the zip file to create.
    source_file_roots: Array of path prefixes to strip off |files| when adding
                       to the archive.
    source_files: The array of files to add to archive. The path structure is
                  preserved (except for the |files_root| prefix).
    gen_files: Full path to binaries to add to archive.
    gen_files_dst: Relative path of where to add binary files in archive.
                   This array needs to parallel |binaries_src|.
    defs: Dictionary of variable definitions.
  """
  cleanDir(temp_dir)

  for f in source_files:
    dst_file = remapSrcFile(temp_dir, source_file_roots, f)
    base_file = os.path.basename(f)
    (base, ext) = os.path.splitext(f)
    if ext == '.zip':
      copyZipIntoArchive(temp_dir, source_file_roots, f)
    elif ext in ['.packproj', '.pkgproj', '.plist', '.props', '.sh', '.json']:
      copyFileWithDefs(f, dst_file, defs)
    else:
      shutil.copy2(f, dst_file)

  for bs, bd in zip(gen_files, gen_files_dst):
    dst_file = os.path.join(temp_dir, bd)
    if not os.path.exists(os.path.dirname(dst_file)):
      os.makedirs(os.path.dirname(dst_file))
    if os.path.isdir(bs):
      shutil.copytree(bs, dst_file)
    else:
      shutil.copy2(bs, dst_file)

  zip_helpers.zip_directory(
    zip_path, temp_dir,
    compress=True,
    zip_prefix_path=os.path.splitext(os.path.basename(zip_path))[0])


def error(msg):
  sys.stderr.write('ERROR: %s\n' % msg)
  sys.exit(1)


def usage():
  """Display basic usage information."""
  print('Usage: %s\n'
        '  <temp-dir> <zip-path>\n'
        '  --source-file-roots <list of roots to strip off source files...>\n'
        '  --source-files <list of source files...>\n'
        '  --generated-files <list of generated target files...>\n'
        '  --generated-files-dst <dst for each generated file...>\n'
        '  --defs <list of VARIABLE=value definitions...>'
        ) % sys.argv[0]


def main():
  if len(sys.argv) < 2:
    usage()
    error('Too few arguments')

  temp_dir = sys.argv[1]
  zip_path = sys.argv[2]

  arg_mode = ''
  source_file_roots = []
  source_files = []
  generated_files = []
  generated_files_dst = []
  definitions = []
  for arg in sys.argv[3:]:
    if arg == '--source-file-roots':
      arg_mode = 'src-roots'
    elif arg == '--source-files':
      arg_mode = 'files'
    elif arg == '--generated-files':
      arg_mode = 'gen-src'
    elif arg == '--generated-files-dst':
      arg_mode = 'gen-dst'
    elif arg == '--defs':
      arg_mode = 'defs'

    elif arg_mode == 'src-roots':
      source_file_roots.append(arg)
    elif arg_mode == 'files':
      source_files.append(arg)
    elif arg_mode == 'gen-src':
      generated_files.append(arg)
    elif arg_mode == 'gen-dst':
      generated_files_dst.append(arg)
    elif arg_mode == 'defs':
      definitions.append(arg)
    else:
      usage()
      error('Expected --source-files')

  # Make sure at least one file was specified.
  if len(source_files) == 0 and len(generated_files) == 0:
    error('At least one input file must be specified.')

  # Sort roots to ensure the longest one is first. See comment in remapSrcFile
  # for why this is necessary.
  source_file_roots = list(map(os.path.normpath, source_file_roots))
  source_file_roots.sort(key=len, reverse=True)

  # Verify that the 2 generated_files arrays have the same number of elements.
  if len(generated_files) != len(generated_files_dst):
    error('len(--generated-files) != len(--generated-files-dst)')

  defs = buildDefDictionary(definitions)

  result = buildHostArchive(temp_dir, zip_path, source_file_roots,
                            source_files, generated_files, generated_files_dst,
                            defs)

  return 0


if __name__ == '__main__':
  sys.exit(main())

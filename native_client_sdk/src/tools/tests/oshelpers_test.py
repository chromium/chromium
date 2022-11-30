#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(PARENT_DIR)

import oshelpers

class RunError(subprocess.CalledProcessError):
  def __init__(self, retcode, command, output, error_output):
    subprocess.CalledProcessError.__init__(self, retcode, command)
    self.output = output
    self.error_output = error_output

  def __str__(self):
    msg = subprocess.CalledProcessError.__str__(self)
    msg += '.\nstdout: """%s"""' % (self.output,)
    msg += '.\nstderr: """%s"""' % (self.error_output,)
    return msg


def RunCmd(cmd, args, cwd, env=None):
  env = env or os.environ
  command = [sys.executable, 'oshelpers.py', cmd] + args
  process = subprocess.Popen(stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             args=command,
                             cwd=cwd,
                             env=env)
  output, error_output = process.communicate()
  retcode = process.returncode

  if retcode:
    raise RunError(retcode, command, output, error_output)
  return output, error_output


class TestZip(unittest.TestCase):
  def setUp(self):
    # make zipname -> "testFooBar.zip"
    self.zipname = self.id().split('.')[-1] + '.zip'
    self.zipfile = None
    self.tempdir = tempfile.mkdtemp()
    shutil.copy(os.path.join(PARENT_DIR, 'oshelpers.py'),
        self.tempdir)

  def tearDown(self):
    if self.zipfile:
      self.zipfile.close()
    shutil.rmtree(self.tempdir)

  def GetTempPath(self, basename):
    return os.path.join(self.tempdir, basename)

  def MakeFile(self, rel_path, size):
    with open(os.path.join(self.tempdir, rel_path), 'wb') as f:
      f.write('0' * size)
    return rel_path

  def RunZip(self, *args):
    return RunCmd('zip', list(args), cwd=self.tempdir)

  def OpenZipFile(self):
    self.zipfile = zipfile.ZipFile(self.GetTempPath(self.zipname), 'r')

  def CloseZipFile(self):
    self.zipfile.close()
    self.zipfile = None

  def GetZipInfo(self, path):
    return self.zipfile.getinfo(oshelpers.OSMakeZipPath(path))

  def testNothingToDo(self):
    self.assertRaises(subprocess.CalledProcessError, self.RunZip,
        self.zipname, 'nonexistent_file')
    self.assertFalse(os.path.exists(self.zipname))

  def testAddSomeFiles(self):
    file1 = self.MakeFile('file1', 1024)
    file2 = self.MakeFile('file2', 3354)
    self.RunZip(self.zipname, file1, file2)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)
    self.assertEqual(self.GetZipInfo(file1).file_size, 1024)
    self.assertEqual(self.GetZipInfo(file2).file_size, 3354)
    # make sure files are added in order
    self.assertEqual(self.zipfile.namelist()[0], file1)

  def testAddFilesWithGlob(self):
    self.MakeFile('file1', 1024)
    self.MakeFile('file2', 3354)
    self.RunZip(self.zipname, 'file*')
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)

  def testAddDir(self):
    os.mkdir(self.GetTempPath('dir1'))
    self.RunZip(self.zipname, 'dir1')
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 1)
    self.assertRaises(KeyError, self.zipfile.getinfo, 'dir1')
    self.zipfile.getinfo('dir1/')

  def testAddRecursive(self):
    os.mkdir(self.GetTempPath('dir1'))
    self.MakeFile(os.path.join('dir1', 'file1'), 256)
    os.mkdir(self.GetTempPath(os.path.join('dir1', 'dir2')))
    self.MakeFile(os.path.join('dir1', 'dir2', 'file2'), 1234)
    self.RunZip(self.zipname, '-r', 'dir1')
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)

  def testUpdate(self):
    file1 = self.MakeFile('file1', 1223)
    self.RunZip(self.zipname, file1)
    self.OpenZipFile()
    self.assertEqual(self.GetZipInfo(file1).file_size, 1223)

    file1 = self.MakeFile('file1', 2334)
    self.RunZip(self.zipname, file1)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 1)
    self.assertEqual(self.GetZipInfo(file1).file_size, 2334)

  def testUpdateOneFileOutOfMany(self):
    file1 = self.MakeFile('file1', 128)
    file2 = self.MakeFile('file2', 256)
    file3 = self.MakeFile('file3', 512)
    file4 = self.MakeFile('file4', 1024)
    self.RunZip(self.zipname, file1, file2, file3, file4)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.CloseZipFile()

    file3 = self.MakeFile('file3', 768)
    self.RunZip(self.zipname, file3)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.zipfile.namelist()[0], file1)
    self.assertEqual(self.GetZipInfo(file1).file_size, 128)
    self.assertEqual(self.zipfile.namelist()[1], file2)
    self.assertEqual(self.GetZipInfo(file2).file_size, 256)
    self.assertEqual(self.zipfile.namelist()[2], file3)
    self.assertEqual(self.GetZipInfo(file3).file_size, 768)
    self.assertEqual(self.zipfile.namelist()[3], file4)
    self.assertEqual(self.GetZipInfo(file4).file_size, 1024)

  def testUpdateSubdirectory(self):
    os.mkdir(self.GetTempPath('dir1'))
    file1 = self.MakeFile(os.path.join('dir1', 'file1'), 256)
    os.mkdir(self.GetTempPath(os.path.join('dir1', 'dir2')))
    self.MakeFile(os.path.join('dir1', 'dir2', 'file2'), 1234)
    self.RunZip(self.zipname, '-r', 'dir1')
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.GetZipInfo(file1).file_size, 256)
    self.CloseZipFile()

    self.MakeFile(file1, 2560)
    self.RunZip(self.zipname, file1)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.GetZipInfo(file1).file_size, 2560)

  def testAppend(self):
    file1 = self.MakeFile('file1', 128)
    file2 = self.MakeFile('file2', 256)
    self.RunZip(self.zipname, file1, file2)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)
    self.CloseZipFile()

    file3 = self.MakeFile('file3', 768)
    self.RunZip(self.zipname, file3)
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 3)


class TestWhich(unittest.TestCase):
  def setUp(self):
    self.path_list = []
    self.tempdir = tempfile.mkdtemp()
    shutil.copy(os.path.join(PARENT_DIR, 'oshelpers.py'),
        self.tempdir)

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def Mkdir(self, path):
    os.mkdir(os.path.join(self.tempdir, path))

  def MakeExecutableFile(self, *path_components):
    path = os.path.join(self.tempdir, *path_components)
    if sys.platform == 'win32':
      path += '.exe'

    with open(path, 'w') as f:
      f.write('')
    os.chmod(path, 0755)

    return path

  def RunWhich(self, *args):
    paths = os.pathsep.join(os.path.join(self.tempdir, p)
                            for p in self.path_list)
    env = {'PATH': paths}
    return RunCmd('which', list(args), cwd=self.tempdir, env=env)

  def testNothing(self):
    self.assertRaises(RunError, self.RunWhich, 'foo')

  def testBasic(self):
    self.Mkdir('bin')
    bin_cp = self.MakeExecutableFile('bin', 'cp')
    cp = os.path.basename(bin_cp)

    self.path_list.append('bin')
    output, _ = self.RunWhich(cp)
    self.assertTrue(os.path.join(self.tempdir, 'bin', cp) in output)

  def testMulti(self):
    self.Mkdir('bin')
    bin_cp = self.MakeExecutableFile('bin', 'cp')
    bin_ls = self.MakeExecutableFile('bin', 'ls')
    cp = os.path.basename(bin_cp)
    ls = os.path.basename(bin_ls)

    self.path_list.append('bin')
    output, _ = self.RunWhich(cp, ls)
    self.assertTrue(os.path.join(self.tempdir, 'bin', cp) in output)
    self.assertTrue(os.path.join(self.tempdir, 'bin', ls) in output)

  def testNonPath(self):
    self.Mkdir('bin')
    bin_cp = self.MakeExecutableFile('bin', 'cp')
    cp = os.path.basename(bin_cp)

    # Note, "bin" not added to PATH.
    output, _ = self.RunWhich(bin_cp)
    self.assertTrue(os.path.join('bin', cp) in output)


if __name__ == '__main__':
  unittest.main()

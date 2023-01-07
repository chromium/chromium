#!/usr/bin/env vpython3
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os
import posixpath
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(TOOLS_DIR, 'lib', 'tests', 'data')
BUILD_TOOLS_DIR = os.path.join(os.path.dirname(TOOLS_DIR), 'build_tools')
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))

sys.path.append(TOOLS_DIR)
sys.path.append(BUILD_TOOLS_DIR)

import build_paths
import create_nmf
import getos
from mock import patch, Mock

TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')
X86_GLIBC_TOOLCHAIN = os.path.join(TOOLCHAIN_OUT,
                                   '%s_x86' % getos.GetPlatform(),
                                   'nacl_x86_glibc')
ARM_GLIBC_TOOLCHAIN = os.path.join(TOOLCHAIN_OUT,
                                   '%s_x86' % getos.GetPlatform(),
                                   'nacl_arm_glibc')

PosixRelPath = create_nmf.PosixRelPath


def StripSo(name):
  """Strip trailing hexidecimal characters from the name of a shared object.

  It strips everything after the last '.' in the name, and checks that the new
  name ends with .so.

  e.g.

  libc.so.ad6acbfa => libc.so
  foo.bar.baz => foo.bar.baz
  """
  if '.' in name:
    stripped_name, ext = name.rsplit('.', 1)
    if stripped_name.endswith('.so') and len(ext) > 1:
      return stripped_name
  return name


class TestPosixRelPath(unittest.TestCase):
  def testBasic(self):
    # Note that PosixRelPath only converts from native path format to posix
    # path format, that's why we have to use os.path.join here.
    path = os.path.join(os.path.sep, 'foo', 'bar', 'baz.blah')
    start = os.path.sep + 'foo'
    self.assertEqual(PosixRelPath(path, start), 'bar/baz.blah')


class TestDefaultLibpath(unittest.TestCase):
  def setUp(self):
    patcher = patch('create_nmf.GetSDKRoot', Mock(return_value='/dummy/path'))
    patcher.start()
    self.addCleanup(patcher.stop)

  def testUsesSDKRoot(self):
    paths = create_nmf.GetDefaultLibPath('Debug')
    for path in paths:
      self.assertTrue(path.startswith('/dummy/path'))

  def testFallbackPath(self):
    paths = create_nmf.GetDefaultLibPath('foo_Debug')
    if sys.platform == 'win32':
      paths = [p.replace('\\', '/') for p in paths]
    path_base = '/dummy/path/lib/glibc_x86_64/foo_Debug'
    path_fallback = '/dummy/path/lib/glibc_x86_64/Debug'
    self.assertIn(path_base, paths)
    self.assertIn(path_fallback, paths)
    self.assertGreater(paths.index(path_fallback), paths.index(path_base))

    paths = create_nmf.GetDefaultLibPath('foo_bar')
    if sys.platform == 'win32':
      paths = [p.replace('\\', '/') for p in paths]
    path_base = '/dummy/path/lib/glibc_x86_64/foo_bar'
    path_fallback = '/dummy/path/lib/glibc_x86_64/Release'
    self.assertIn(path_base, paths)
    self.assertIn(path_fallback, paths)
    self.assertGreater(paths.index(path_fallback), paths.index(path_base))


class TestNmfUtils(unittest.TestCase):
  """Tests for the main NmfUtils class in create_nmf."""

  def setUp(self):
    self.tempdir = None
    self.objdump = os.path.join(X86_GLIBC_TOOLCHAIN, 'bin', 'i686-nacl-objdump')
    if os.name == 'nt':
      self.objdump += '.exe'
    self._Mktemp()

    # Create dummy elf_loader_arm.nexe by duplicating an existing so.
    # This nexe is normally build during SDK build but we want these tests
    # to run standalone, and the contents of the ELF are not important for
    # these tests.
    arm_libdir = os.path.join(ARM_GLIBC_TOOLCHAIN, 'arm-nacl', 'lib')
    shutil.copy(os.path.join(arm_libdir, 'ld-nacl-arm.so.1'),
        os.path.join(arm_libdir, 'elf_loader_arm.nexe'))


  def _CreateTestNexe(self, name, arch):
    """Create an empty test .nexe file for use in create_nmf tests.

    This is used rather than checking in test binaries since the
    checked in binaries depend on .so files that only exist in the
    certain SDK that build them.
    """
    if arch == 'arm':
      toolchain = ARM_GLIBC_TOOLCHAIN
    else:
      toolchain = X86_GLIBC_TOOLCHAIN

    compiler = os.path.join(toolchain, 'bin', '%s-nacl-g++' % arch)
    if os.name == 'nt':
      compiler += '.exe'
      os.environ['CYGWIN'] = 'nodosfilewarning'
    program = 'int main() { return 0; }'
    name = os.path.join(self.tempdir, name)
    dst_dir = os.path.dirname(name)
    if not os.path.exists(dst_dir):
      os.makedirs(dst_dir)
    self.assertTrue(os.path.exists(compiler), 'compiler missing: %s' % compiler)
    cmd = [compiler, '-pthread', '-x' , 'c', '-o', name, '-']
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    p.communicate(input=program)
    self.assertEqual(p.returncode, 0)
    return name

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def _Mktemp(self):
    self.tempdir = tempfile.mkdtemp()

  def _CreateNmfUtils(self, nexes, **kwargs):
    if not kwargs.get('lib_path'):
      kwargs['lib_path'] = [
          # Use lib instead of lib64 (lib64 is a symlink to lib).
          os.path.join(X86_GLIBC_TOOLCHAIN, 'x86_64-nacl', 'lib'),
          os.path.join(X86_GLIBC_TOOLCHAIN, 'x86_64-nacl', 'lib32'),
          os.path.join(ARM_GLIBC_TOOLCHAIN, 'arm-nacl', 'lib')]
    return create_nmf.NmfUtils(nexes,
                               objdump=self.objdump,
                               **kwargs)

  def _CreateStatic(self, arch_path=None, **kwargs):
    """Copy all static .nexe files from the DATA_DIR to a temporary directory.

    Args:
      arch_path: A dictionary mapping architecture to the directory to generate
          the .nexe for the architecture in.
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .nexe paths
    """
    arch_path = arch_path or {}
    nexes = []
    for arch in ('x86_64', 'x86_32', 'arm'):
      nexe_name = 'test_static_%s.nexe' % arch
      src_nexe = os.path.join(DATA_DIR, nexe_name)
      dst_nexe = os.path.join(self.tempdir, arch_path.get(arch, ''), nexe_name)
      dst_dir = os.path.dirname(dst_nexe)
      if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
      shutil.copy(src_nexe, dst_nexe)
      nexes.append(dst_nexe)

    nexes.sort()
    nmf_utils = self._CreateNmfUtils(nexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())
    return nmf, nexes

  def _CreateDynamicAndStageDeps(self, arch_path=None, **kwargs):
    """Create dynamic .nexe files and put them in a temporary directory, with
    their dependencies staged in the same directory.

    Args:
      arch_path: A dictionary mapping architecture to the directory to generate
          the .nexe for the architecture in.
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .nexe paths
    """
    arch_path = arch_path or {}
    nexes = []
    for arch in ('x86_64', 'x86_32', 'arm'):
      nexe_name = 'test_dynamic_%s.nexe' % arch
      rel_nexe = os.path.join(arch_path.get(arch, ''), nexe_name)
      arch_alt = 'i686' if arch == 'x86_32' else arch
      nexe = self._CreateTestNexe(rel_nexe, arch_alt)
      nexes.append(nexe)

    nexes.sort()
    nmf_utils = self._CreateNmfUtils(nexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())
    nmf_utils.StageDependencies(self.tempdir)

    return nmf, nexes

  def _CreatePexe(self, **kwargs):
    """Copy test.pexe from the DATA_DIR to a temporary directory.

    Args:
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .pexe paths
    """
    pexe_name = 'test.pexe'
    src_pexe = os.path.join(DATA_DIR, pexe_name)
    dst_pexe = os.path.join(self.tempdir, pexe_name)
    shutil.copy(src_pexe, dst_pexe)

    pexes = [dst_pexe]
    nmf_utils = self._CreateNmfUtils(pexes, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())

    return nmf, pexes

  def _CreateBitCode(self, **kwargs):
    """Copy test.bc from the DATA_DIR to a temporary directory.

    Args:
      kwargs: Keyword arguments to pass through to create_nmf.NmfUtils
          constructor.

    Returns:
      A tuple with 2 elements:
        * The generated NMF as a dictionary (i.e. parsed by json.loads)
        * A list of the generated .bc paths
    """
    bc_name = 'test.bc'
    src_bc = os.path.join(DATA_DIR, bc_name)
    dst_bc = os.path.join(self.tempdir, bc_name)
    shutil.copy(src_bc, dst_bc)

    bcs = [dst_bc]
    nmf_utils = self._CreateNmfUtils(bcs, **kwargs)
    nmf = json.loads(nmf_utils.GetJson())

    return nmf, bcs

  def assertManifestEquals(self, manifest, expected):
    """Compare two manifest dictionaries.

    The input manifest is regenerated with all string keys and values being
    processed through StripSo, to remove the random hexidecimal characters at
    the end of shared object names.

    Args:
      manifest: The generated manifest.
      expected: The expected manifest.
    """
    def StripSoCopyDict(d):
      new_d = {}
      for k, v in d.iteritems():
        new_k = StripSo(k)
        if isinstance(v, (str, unicode)):
          new_v = StripSo(v)
        elif isinstance(v, list):
          new_v = v[:]
        elif isinstance(v, dict):
          new_v = StripSoCopyDict(v)
        else:
          # Assume that anything else can be copied directly.
          new_v = v

        new_d[new_k] = new_v
      return new_d

    strip_manifest = StripSoCopyDict(manifest)
    self.assertEqual(strip_manifest['program'], expected['program'])
    if 'files' in strip_manifest:
      for key in strip_manifest['files']:
        self.assertEqual(strip_manifest['files'][key], expected['files'][key])

    self.assertEqual(strip_manifest, expected)

  def assertStagingEquals(self, expected):
    """Compare the contents of the temporary directory, to an expected
    directory layout.

    Args:
      expected: The expected directory layout.
    """
    all_files = []
    for root, _, files in os.walk(self.tempdir):
      rel_root_posix = PosixRelPath(root, self.tempdir)
      for f in files:
        path = posixpath.join(rel_root_posix, StripSo(f))
        if path.startswith('./'):
          path = path[2:]
        all_files.append(path)
    self.assertEqual(set(expected), set(all_files))

  arch_dir = {'x86_32': 'x86_32', 'x86_64': 'x86_64', 'arm': 'arm'}

  def testStatic(self):
    nmf, _ = self._CreateStatic()
    expected_manifest = {
      'files': {},
      'program': {
        'x86-64': {'url': 'test_static_x86_64.nexe'},
        'x86-32': {'url': 'test_static_x86_32.nexe'},
        'arm': {'url': 'test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithPath(self):
    nmf, _ = self._CreateStatic(self.arch_dir, nmf_root=self.tempdir)
    expected_manifest = {
      'files': {},
      'program': {
        'x86-32': {'url': 'x86_32/test_static_x86_32.nexe'},
        'x86-64': {'url': 'x86_64/test_static_x86_64.nexe'},
        'arm': {'url': 'arm/test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithPathNoNmfRoot(self):
    # This case is not particularly useful, but it is similar to how create_nmf
    # used to work. If there is no nmf_root given, all paths are relative to
    # the first nexe passed on the commandline. I believe the assumption
    # previously was that all .nexes would be in the same directory.
    nmf, _ = self._CreateStatic(self.arch_dir)
    expected_manifest = {
      'files': {},
      'program': {
        'x86-32': {'url': '../x86_32/test_static_x86_32.nexe'},
        'x86-64': {'url': '../x86_64/test_static_x86_64.nexe'},
        'arm': {'url': 'test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testStaticWithNexePrefix(self):
    nmf, _ = self._CreateStatic(nexe_prefix='foo')
    expected_manifest = {
      'files': {},
      'program': {
        'x86-64': {'url': 'foo/test_static_x86_64.nexe'},
        'x86-32': {'url': 'foo/test_static_x86_32.nexe'},
        'arm': {'url': 'foo/test_static_arm.nexe'},
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testDynamic(self):
    nmf, nexes = self._CreateDynamicAndStageDeps()
    expected_manifest = {
      'files': {
        'main.nexe': {
          'x86-32': {'url': 'test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'test_dynamic_x86_64.nexe'},
          'arm': {'url': 'test_dynamic_arm.nexe'},
        },
        'ld-nacl-arm.so.1': {
          'arm': {'url': 'libarm/ld-nacl-arm.so.1'},
        },
        'libc.so.0.1': {
          'arm': {'url': 'libarm/libc.so.0.1'}
        },
        'libc.so': {
          'x86-32': {'url': 'lib32/libc.so'},
          'x86-64': {'url': 'lib64/libc.so'},
        },
        'libgcc_s.so.1': {
          'arm': {'url': 'libarm/libgcc_s.so.1'},
          'x86-32': {'url': 'lib32/libgcc_s.so.1'},
          'x86-64': {'url': 'lib64/libgcc_s.so.1'},
        },
        'libpthread.so.0': {
          'arm': { 'url': 'libarm/libpthread.so.0'}
        },
        'libpthread.so': {
          'x86-32': {'url': 'lib32/libpthread.so'},
          'x86-64': {'url': 'lib64/libpthread.so'},
        },
      },
      'program': {
        'arm': {'url': 'libarm/elf_loader_arm.nexe'},
        'x86-32': {'url': 'lib32/runnable-ld.so'},
        'x86-64': {'url': 'lib64/runnable-ld.so'},
      }
    }

    expected_staging = [os.path.basename(f) for f in nexes]
    expected_staging.extend([
      'lib32/libc.so',
      'lib32/libgcc_s.so.1',
      'lib32/libpthread.so',
      'lib32/runnable-ld.so',
      'lib64/libc.so',
      'lib64/libgcc_s.so.1',
      'lib64/libpthread.so',
      'lib64/runnable-ld.so',
      'libarm/elf_loader_arm.nexe',
      'libarm/libpthread.so.0',
      'libarm/ld-nacl-arm.so.1',
      'libarm/libgcc_s.so.1',
      'libarm/libc.so.0.1'
    ])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithPath(self):
    nmf, nexes = self._CreateDynamicAndStageDeps(self.arch_dir,
                                                 nmf_root=self.tempdir)
    expected_manifest = {
      'files': {
        'main.nexe': {
          'arm': {'url': 'arm/test_dynamic_arm.nexe'},
          'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
        },
        'libc.so.0.1': {
          'arm': {'url': 'arm/libarm/libc.so.0.1'}
        },
        'ld-nacl-arm.so.1': {
          'arm': {'url': 'arm/libarm/ld-nacl-arm.so.1'},
        },
        'libc.so': {
          'x86-32': {'url': 'x86_32/lib32/libc.so'},
          'x86-64': {'url': 'x86_64/lib64/libc.so'},
        },
        'libgcc_s.so.1': {
          'arm': {'url': 'arm/libarm/libgcc_s.so.1'},
          'x86-32': {'url': 'x86_32/lib32/libgcc_s.so.1'},
          'x86-64': {'url': 'x86_64/lib64/libgcc_s.so.1'},
        },
        'libpthread.so.0': {
          'arm': { 'url': 'arm/libarm/libpthread.so.0'}
        },
        'libpthread.so': {
          'x86-32': {'url': 'x86_32/lib32/libpthread.so'},
          'x86-64': {'url': 'x86_64/lib64/libpthread.so'},
        },
      },
      'program': {
        'arm': {'url': 'arm/libarm/elf_loader_arm.nexe'},
        'x86-32': {'url': 'x86_32/lib32/runnable-ld.so'},
        'x86-64': {'url': 'x86_64/lib64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'x86_32/lib32/libc.so',
      'x86_32/lib32/libgcc_s.so.1',
      'x86_32/lib32/libpthread.so',
      'x86_32/lib32/runnable-ld.so',
      'x86_64/lib64/libc.so',
      'x86_64/lib64/libgcc_s.so.1',
      'x86_64/lib64/libpthread.so',
      'x86_64/lib64/runnable-ld.so',
      'arm/libarm/elf_loader_arm.nexe',
      'arm/libarm/libpthread.so.0',
      'arm/libarm/ld-nacl-arm.so.1',
      'arm/libarm/libgcc_s.so.1',
      'arm/libarm/libc.so.0.1'
    ])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithRelPath(self):
    """Test that when the nmf root is a relative path that things work."""
    old_path = os.getcwd()
    try:
      os.chdir(self.tempdir)
      nmf, nexes = self._CreateDynamicAndStageDeps(self.arch_dir, nmf_root='')
      expected_manifest = {
        'files': {
          'main.nexe': {
            'arm': {'url': 'arm/test_dynamic_arm.nexe'},
            'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
            'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
          },
          'ld-nacl-arm.so.1': {
            'arm': {'url': 'arm/libarm/ld-nacl-arm.so.1'},
          },
          'libc.so.0.1': {
            'arm': {'url': 'arm/libarm/libc.so.0.1'}
          },
          'libc.so': {
            'x86-32': {'url': 'x86_32/lib32/libc.so'},
            'x86-64': {'url': 'x86_64/lib64/libc.so'},
          },
          'libgcc_s.so.1': {
            'arm': {'url': 'arm/libarm/libgcc_s.so.1'},
            'x86-32': {'url': 'x86_32/lib32/libgcc_s.so.1'},
            'x86-64': {'url': 'x86_64/lib64/libgcc_s.so.1'},
          },
          'libpthread.so.0': {
            'arm': { 'url': 'arm/libarm/libpthread.so.0'}
          },
          'libpthread.so': {
            'x86-32': {'url': 'x86_32/lib32/libpthread.so'},
            'x86-64': {'url': 'x86_64/lib64/libpthread.so'},
          },
        },
        'program': {
          'arm': {'url': 'arm/libarm/elf_loader_arm.nexe'},
          'x86-32': {'url': 'x86_32/lib32/runnable-ld.so'},
          'x86-64': {'url': 'x86_64/lib64/runnable-ld.so'},
        }
      }

      expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
      expected_staging.extend([
        'x86_32/lib32/libc.so',
        'x86_32/lib32/libgcc_s.so.1',
        'x86_32/lib32/libpthread.so',
        'x86_32/lib32/runnable-ld.so',
        'x86_64/lib64/libc.so',
        'x86_64/lib64/libgcc_s.so.1',
        'x86_64/lib64/libpthread.so',
        'x86_64/lib64/runnable-ld.so',
        'arm/libarm/elf_loader_arm.nexe',
        'arm/libarm/libpthread.so.0',
        'arm/libarm/ld-nacl-arm.so.1',
        'arm/libarm/libgcc_s.so.1',
        'arm/libarm/libc.so.0.1'
      ])

      self.assertManifestEquals(nmf, expected_manifest)
      self.assertStagingEquals(expected_staging)
    finally:
      os.chdir(old_path)

  def testDynamicWithPathNoArchPrefix(self):
    nmf, nexes = self._CreateDynamicAndStageDeps(self.arch_dir,
                                                 nmf_root=self.tempdir,
                                                 no_arch_prefix=True)
    expected_manifest = {
      'files': {
        'main.nexe': {
          'arm': {'url': 'arm/test_dynamic_arm.nexe'},
          'x86-32': {'url': 'x86_32/test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'x86_64/test_dynamic_x86_64.nexe'},
        },
        'ld-nacl-arm.so.1': {
          'arm': {'url': 'arm/ld-nacl-arm.so.1'},
        },
        'libc.so.0.1': {
          'arm': {'url': 'arm/libc.so.0.1'}
        },
        'libc.so': {
          'x86-32': {'url': 'x86_32/libc.so'},
          'x86-64': {'url': 'x86_64/libc.so'},
        },
        'libgcc_s.so.1': {
          'arm': {'url': 'arm/libgcc_s.so.1'},
          'x86-32': {'url': 'x86_32/libgcc_s.so.1'},
          'x86-64': {'url': 'x86_64/libgcc_s.so.1'},
        },
        'libpthread.so.0': {
          'arm': { 'url': 'arm/libpthread.so.0'}
        },
        'libpthread.so': {
          'x86-32': {'url': 'x86_32/libpthread.so'},
          'x86-64': {'url': 'x86_64/libpthread.so'},
        },
      },
      'program': {
        'arm': {'url': 'arm/elf_loader_arm.nexe'},
        'x86-32': {'url': 'x86_32/runnable-ld.so'},
        'x86-64': {'url': 'x86_64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'x86_32/libc.so',
      'x86_32/libgcc_s.so.1',
      'x86_32/libpthread.so',
      'x86_32/runnable-ld.so',
      'x86_64/libc.so',
      'x86_64/libgcc_s.so.1',
      'x86_64/libpthread.so',
      'x86_64/runnable-ld.so',
      'arm/elf_loader_arm.nexe',
      'arm/libpthread.so.0',
      'arm/ld-nacl-arm.so.1',
      'arm/libgcc_s.so.1',
      'arm/libc.so.0.1'
    ])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testDynamicWithLibPrefix(self):
    nmf, nexes = self._CreateDynamicAndStageDeps(lib_prefix='foo')
    expected_manifest = {
      'files': {
        'main.nexe': {
          'arm': {'url': 'test_dynamic_arm.nexe'},
          'x86-32': {'url': 'test_dynamic_x86_32.nexe'},
          'x86-64': {'url': 'test_dynamic_x86_64.nexe'},
        },
        'ld-nacl-arm.so.1': {
          'arm': {'url': 'foo/libarm/ld-nacl-arm.so.1'},
        },
        'libc.so.0.1': {
          'arm': {'url': 'foo/libarm/libc.so.0.1'}
        },
        'libc.so': {
          'x86-32': {'url': 'foo/lib32/libc.so'},
          'x86-64': {'url': 'foo/lib64/libc.so'},
        },
        'libgcc_s.so.1': {
          'arm': {'url': 'foo/libarm/libgcc_s.so.1'},
          'x86-32': {'url': 'foo/lib32/libgcc_s.so.1'},
          'x86-64': {'url': 'foo/lib64/libgcc_s.so.1'},
        },
        'libpthread.so.0': {
          'arm': { 'url': 'foo/libarm/libpthread.so.0'}
        },
        'libpthread.so': {
          'x86-32': {'url': 'foo/lib32/libpthread.so'},
          'x86-64': {'url': 'foo/lib64/libpthread.so'},
        },
      },
      'program': {
        'arm': {'url': 'foo/libarm/elf_loader_arm.nexe'},
        'x86-32': {'url': 'foo/lib32/runnable-ld.so'},
        'x86-64': {'url': 'foo/lib64/runnable-ld.so'},
      }
    }

    expected_staging = [PosixRelPath(f, self.tempdir) for f in nexes]
    expected_staging.extend([
      'foo/lib32/libc.so',
      'foo/lib32/libgcc_s.so.1',
      'foo/lib32/libpthread.so',
      'foo/lib32/runnable-ld.so',
      'foo/lib64/libc.so',
      'foo/lib64/libgcc_s.so.1',
      'foo/lib64/libpthread.so',
      'foo/lib64/runnable-ld.so',
      'foo/libarm/elf_loader_arm.nexe',
      'foo/libarm/libpthread.so.0',
      'foo/libarm/ld-nacl-arm.so.1',
      'foo/libarm/libgcc_s.so.1',
      'foo/libarm/libc.so.0.1'
    ])

    self.assertManifestEquals(nmf, expected_manifest)
    self.assertStagingEquals(expected_staging)

  def testPexe(self):
    nmf, _ = self._CreatePexe()
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-translate': {
            'url': 'test.pexe'
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testPexeOptLevel(self):
    nmf, _ = self._CreatePexe(pnacl_optlevel=2)
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-translate': {
            'url': 'test.pexe',
            'optlevel': 2,
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)

  def testBitCode(self):
    nmf, _ = self._CreateBitCode(pnacl_debug_optlevel=0)
    expected_manifest = {
      'program': {
        'portable': {
          'pnacl-debug': {
            'url': 'test.bc',
            'optlevel': 0,
          }
        }
      }
    }
    self.assertManifestEquals(nmf, expected_manifest)


if __name__ == '__main__':
  unittest.main()

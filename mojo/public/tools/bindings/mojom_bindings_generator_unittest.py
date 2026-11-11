# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import shutil
import subprocess
import sys
import unittest

from mojom_bindings_generator import MakeImportStackMessage
from mojom_bindings_generator import MojomProcessor
from mojom_bindings_generator import RelativePath
from mojom_bindings_generator import ScrambleMethodOrdinals
from mojom_bindings_generator import _CreateArgumentParser
from mojom_bindings_generator import main
from mojom_parser_test_case import MojomParserTestCase


def _FindGnBinary():
  if sys.platform.startswith('linux'):
    subdir, exe = 'linux64', 'gn'
  elif sys.platform == 'darwin':
    subdir, exe = 'mac', 'gn'
  elif sys.platform == 'win32':
    subdir, exe = 'win', 'gn.exe'
  else:
    subdir, exe = None, None

  candidates = []
  if subdir:
    src_dir = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
    )
    candidates.append(os.path.join(src_dir, 'buildtools', subdir, exe))
  which_gn = shutil.which('gn')
  if which_gn:
    candidates.append(which_gn)

  for candidate in candidates:
    if os.path.isfile(candidate):
      try:
        res = subprocess.run(
          [candidate, '--version'],
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          check=False,
        )
        if res.returncode == 0:
          return candidate
      except OSError:
        continue
  return None


class FakeIface:
  def __init__(self):
    self.mojom_name = None
    self.methods = None


class FakeMethod:
  def __init__(self, explicit_ordinal=None):
    self.explicit_ordinal = explicit_ordinal
    self.ordinal = explicit_ordinal
    self.ordinal_comment = None


class MojoBindingsGeneratorTest(unittest.TestCase):
  """Tests mojo_bindings_generator."""

  def testMakeImportStackMessage(self):
    """Tests MakeImportStackMessage()."""
    self.assertEqual(MakeImportStackMessage(["x"]), "")
    self.assertEqual(
      MakeImportStackMessage(["x", "y"]), "\n  y was imported by x"
    )
    self.assertEqual(
      MakeImportStackMessage(["x", "y", "z"]),
      "\n  z was imported by y\n  y was imported by x",
    )

  def testScrambleMethodOrdinals(self):
    """Tests ScrambleMethodOrdinals()."""
    interface = FakeIface()
    interface.mojom_name = 'RendererConfiguration'
    interface.methods = [
      FakeMethod(),
      FakeMethod(),
      FakeMethod(),
      FakeMethod(explicit_ordinal=42),
    ]
    ScrambleMethodOrdinals([interface], "foo".encode('utf-8'))
    # These next three values are hard-coded. If the generation algorithm
    # changes from being based on sha256(seed + interface.name + str(i)) then
    # these numbers will obviously need to change too.
    #
    # Note that hashlib.sha256('fooRendererConfiguration1').digest()[:4] is
    # '\xa5\xbc\xf9\xca' and that hex(1257880741) = '0x4af9bca5'. The
    # difference in 0x4a vs 0xca is because we only take 31 bits.
    self.assertEqual(interface.methods[0].ordinal, 1257880741)
    self.assertEqual(interface.methods[1].ordinal, 631133653)
    self.assertEqual(interface.methods[2].ordinal, 549336076)

    # Explicit method ordinals should not be scrambled.
    self.assertEqual(interface.methods[3].ordinal, 42)

  def testScrambleMethodOrdinalsRejectsEmptySalt(self):
    """Tests that ScrambleMethodOrdinals() rejects empty or whitespace salts."""
    for invalid_salt in (None, b'', b'   \n\t\r'):
      with self.assertRaises(ValueError):
        ScrambleMethodOrdinals([], invalid_salt)


class MojoBindingsGeneratorSaltCliTest(MojomParserTestCase):
  """Tests CLI salt flags in mojom_bindings_generator."""

  _TEST_MOJOM = """
    module test.mojom;
    interface RendererConfiguration {
      Method0();
      Method1();
      Method2();
      Method3@42();
    };
  """

  def setUp(self):
    super().setUp()
    self.WriteFile('test.mojom', self._TEST_MOJOM)
    self.ParseMojoms(['test.mojom'])

  def _GenerateOrdinals(self, extra_args):
    mojom_path = self.GetPath('test.mojom')
    args, remaining_args = _CreateArgumentParser().parse_known_args(
      [
        '-o',
        self.GetPath('out'),
        'generate',
        '-d',
        self.GetPath(''),
        '-g',
        '',
        '-c',
        '',
        '--bytecode_path',
        '',
        *extra_args,
        mojom_path,
      ]
    )
    processor = MojomProcessor(args.filename)
    result = processor._GenerateModule(
      args,
      remaining_args,
      {},
      {},
      {},
      RelativePath(mojom_path, args.depth, args.output_dir),
      [],
    )
    return [m.ordinal for m in result.module.interfaces[0].methods]

  def testNoSaltLeavesOrdinalsUnscrambled(self):
    self.assertEqual(self._GenerateOrdinals([]), [0, 1, 2, 42])

  def testLiteralSaltArgument(self):
    self.assertEqual(
      self._GenerateOrdinals(['--scrambled_message_id_salt', 'foo']),
      [1257880741, 631133653, 549336076, 42],
    )

  def testFilePathSaltArgument(self):
    self.WriteFile('salt.txt', 'foo')
    self.assertEqual(
      self._GenerateOrdinals(
        ['--scrambled_message_id_salt_path', self.GetPath('salt.txt')]
      ),
      [1257880741, 631133653, 549336076, 42],
    )

  def testCombinedLiteralAndFilePathSalts(self):
    self.WriteFile('salt_o.txt', 'o')
    self.assertEqual(
      self._GenerateOrdinals(
        [
          '--scrambled_message_id_salt',
          'f',
          '--scrambled_message_id_salt',
          'o',
          '--scrambled_message_id_salt_path',
          self.GetPath('salt_o.txt'),
        ]
      ),
      [1257880741, 631133653, 549336076, 42],
    )

  def testMainRejectsEmptyOrWhitespaceSalts(self):
    self.WriteFile('empty.txt', '')
    self.WriteFile('whitespace.txt', '  \n\t')
    for bad_args in (
      ['--scrambled_message_id_salt', ''],
      ['--scrambled_message_id_salt', '   \n\t'],
      ['--scrambled_message_id_salt_path', self.GetPath('empty.txt')],
      ['--scrambled_message_id_salt_path', self.GetPath('whitespace.txt')],
    ):
      with self.assertRaises(ValueError):
        main(
          [
            '-o',
            self.GetPath('out'),
            'generate',
            '-d',
            self.GetPath(''),
            '-g',
            '',
            '-c',
            '',
            '--bytecode_path',
            '',
            *bad_args,
            self.GetPath('test.mojom'),
          ]
        )

  def _SetupGnRoot(self):
    gn_bin = _FindGnBinary()
    if not gn_bin:
      self.skipTest('gn binary not available in this test environment')
    gn_root = self.GetPath('gn_root')
    gni_dest_dir = os.path.join(gn_root, 'mojo', 'public', 'tools', 'bindings')
    os.makedirs(gni_dest_dir, exist_ok=True)
    os.makedirs(os.path.join(gn_root, 'chrome'), exist_ok=True)
    shutil.copyfile(
      os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        'mojom_message_id_scrambling.gni',
      ),
      os.path.join(gni_dest_dir, 'mojom_message_id_scrambling.gni'),
    )
    self.WriteFile('gn_root/.gn', 'buildconfig = "//BUILDCONFIG.gn"\n')
    self.WriteFile(
      'gn_root/BUILDCONFIG.gn', 'set_default_toolchain("//:toolchain")\n'
    )
    self.WriteFile(
      'gn_root/BUILD.gn',
      """
      import("//mojo/public/tools/bindings/mojom_message_id_scrambling.gni")
      toolchain("toolchain") {
        tool("stamp") {
          command = "true"
        }
      }
      write_file("$root_build_dir/scrambling.json",
                 {
                   args = message_scrambling_args
                   inputs = message_scrambling_inputs
                 },
                 "json")
      group("default") {}
      """,
    )
    return gn_bin, gn_root

  def _RunGnWithVersion(self, gn_bin, gn_root, version_contents):
    self.WriteFile('gn_root/chrome/VERSION', version_contents)
    out_dir = os.path.join(gn_root, 'out')
    return subprocess.run(
      [gn_bin, 'gen', f'--root={gn_root}', out_dir],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      encoding='utf-8',
      errors='replace',
      check=False,
    )

  def testVersionPatchBumpGeneratesSameSaltAndOrdinals(self):
    gn_bin, gn_root = self._SetupGnRoot()
    version_patch_0 = 'MAJOR=159\nMINOR=0\nBUILD=8148\nPATCH=0\n'
    version_patch_10 = 'MAJOR=159\nMINOR=0\nBUILD=8148\nPATCH=10\n'

    proc_0 = self._RunGnWithVersion(gn_bin, gn_root, version_patch_0)
    self.assertEqual(proc_0.returncode, 0, proc_0.stdout + proc_0.stderr)
    with open(os.path.join(gn_root, 'out', 'scrambling.json')) as f:
      scrambling_0 = json.load(f)

    proc_10 = self._RunGnWithVersion(gn_bin, gn_root, version_patch_10)
    self.assertEqual(proc_10.returncode, 0, proc_10.stdout + proc_10.stderr)
    with open(os.path.join(gn_root, 'out', 'scrambling.json')) as f:
      scrambling_10 = json.load(f)

    self.assertEqual(
      scrambling_0,
      {'args': ['--scrambled_message_id_salt', '8148'], 'inputs': []},
    )
    self.assertEqual(scrambling_0, scrambling_10)

    ordinals_0 = self._GenerateOrdinals(scrambling_0['args'])
    ordinals_10 = self._GenerateOrdinals(scrambling_10['args'])
    self.assertEqual(ordinals_0, ordinals_10)
    self.assertNotEqual(ordinals_0, [0, 1, 2, 42])

  def testVersionBuildBumpGeneratesDifferentSaltAndOrdinals(self):
    gn_bin, gn_root = self._SetupGnRoot()
    proc_a = self._RunGnWithVersion(
      gn_bin, gn_root, 'MAJOR=159\nMINOR=0\nBUILD=8148\nPATCH=0\n'
    )
    self.assertEqual(proc_a.returncode, 0, proc_a.stdout + proc_a.stderr)
    with open(os.path.join(gn_root, 'out', 'scrambling.json')) as f:
      scrambling_a = json.load(f)

    proc_b = self._RunGnWithVersion(
      gn_bin, gn_root, 'MAJOR=159\nMINOR=0\nBUILD=8149\nPATCH=0\n'
    )
    self.assertEqual(proc_b.returncode, 0, proc_b.stdout + proc_b.stderr)
    with open(os.path.join(gn_root, 'out', 'scrambling.json')) as f:
      scrambling_b = json.load(f)

    self.assertEqual(
      scrambling_a['args'], ['--scrambled_message_id_salt', '8148']
    )
    self.assertEqual(
      scrambling_b['args'], ['--scrambled_message_id_salt', '8149']
    )
    self.assertNotEqual(
      self._GenerateOrdinals(scrambling_a['args']),
      self._GenerateOrdinals(scrambling_b['args']),
    )

  def testVersionMissingOrEmptyBuildFailsGnGen(self):
    gn_bin, gn_root = self._SetupGnRoot()
    for bad_version in (
      'MAJOR=159\nMINOR=0\nPATCH=0\n',
      'MAJOR=159\nMINOR=0\nBUILD=\nPATCH=0\n',
      'MAJOR=159\nMINOR=0\nBUILD\nPATCH=0\n',
    ):
      proc = self._RunGnWithVersion(gn_bin, gn_root, bad_version)
      self.assertNotEqual(proc.returncode, 0)
      self.assertIn(
        'Failed to parse BUILD= from //chrome/VERSION',
        proc.stdout + proc.stderr,
      )


if __name__ == "__main__":
  unittest.main()

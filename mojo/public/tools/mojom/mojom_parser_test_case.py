# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import os.path
import shutil
import tempfile
import unittest

import mojom_parser

from mojom.generate import module


class MojomParserTestCase(unittest.TestCase):
  """Tests covering the behavior defined by the main mojom_parser.py script.
  This includes behavior around input and output path manipulation, dependency
  resolution, and module serialization and deserialization."""

  def __init__(self, method_name):
    super().__init__(method_name)
    self._temp_dir = None

  def setUp(self):
    self._temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self._temp_dir)
    self._temp_dir = None

  def GetPath(self, path):
    assert not os.path.isabs(path)
    return os.path.join(self._temp_dir, path)

  def GetModulePath(self, path):
    assert not os.path.isabs(path)
    return os.path.join(self.GetPath('out'), path) + '-module'

  def WriteFile(self, path, contents):
    full_path = self.GetPath(path)
    dirname = os.path.dirname(full_path)
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    with open(full_path, 'w') as f:
      f.write(contents)

  def LoadModule(self, mojom_path):
    with open(self.GetModulePath(mojom_path), 'rb') as f:
      return module.Module.Load(f)

  def ParseMojoms(self, mojoms, metadata=None):
    """Parse all input mojoms relative the temp dir."""
    out_dir = self.GetPath('out')
    args = [
        '--input-root', self._temp_dir, '--input-root', out_dir,
        '--output-root', out_dir, '--mojoms'
    ] + list(map(lambda mojom: os.path.join(self._temp_dir, mojom), mojoms))
    if metadata:
      args.extend(['--check-imports', self.GetPath(metadata)])
    mojom_parser.Run(args)

  def ExtractTypes(self, mojom):
    filename = 'test.mojom'
    self.WriteFile(filename, mojom)
    self.ParseMojoms([filename])
    m = self.LoadModule(filename)
    definitions = {}
    for kinds in (m.enums, m.structs, m.unions, m.interfaces, m.features):
      for kind in kinds:
        definitions[kind.mojom_name] = kind
    return definitions

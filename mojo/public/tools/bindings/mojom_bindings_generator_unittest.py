# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from mojom_bindings_generator import MakeImportStackMessage
from mojom_bindings_generator import ScrambleMethodOrdinals


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
    self.assertEqual(MakeImportStackMessage(["x", "y"]),
                     "\n  y was imported by x")
    self.assertEqual(MakeImportStackMessage(["x", "y", "z"]),
                     "\n  z was imported by y\n  y was imported by x")

  def testScrambleMethodOrdinals(self):
    """Tests ScrambleMethodOrdinals()."""
    interface = FakeIface()
    interface.mojom_name = 'RendererConfiguration'
    interface.methods = [
        FakeMethod(),
        FakeMethod(),
        FakeMethod(),
        FakeMethod(explicit_ordinal=42)
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


if __name__ == "__main__":
  unittest.main()

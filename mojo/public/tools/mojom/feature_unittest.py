# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from mojom_parser_test_case import MojomParserTestCase


class FeatureTest(MojomParserTestCase):
  """Tests feature parsing behavior."""
  def testFeatureOff(self):
    """Verifies basic parsing of feature types."""
    types = self.ExtractTypes("""
      // e.g. BASE_DECLARE_FEATURE(kFeature);
      [AttributeOne=ValueOne]
      feature kFeature {
        // BASE_FEATURE(kFeature,"MyFeature",
        //     base::FEATURE_DISABLED_BY_DEFAULT);
        const string name = "MyFeature";
        const bool default_state = false;
      };
    """)
    self.assertEqual('name', types['kFeature'].constants[0].mojom_name)
    self.assertEqual('"MyFeature"', types['kFeature'].constants[0].value)
    self.assertEqual('default_state', types['kFeature'].constants[1].mojom_name)
    self.assertEqual('false', types['kFeature'].constants[1].value)

  def testFeatureOn(self):
    """Verifies basic parsing of feature types."""
    types = self.ExtractTypes("""
      // e.g. BASE_DECLARE_FEATURE(kFeature);
      feature kFeature {
        // BASE_FEATURE(kFeature,"MyFeature",
        //     base::FEATURE_ENABLED_BY_DEFAULT);
        const string name = "MyFeature";
        const bool default_state = true;
      };
    """)
    self.assertEqual('name', types['kFeature'].constants[0].mojom_name)
    self.assertEqual('"MyFeature"', types['kFeature'].constants[0].value)
    self.assertEqual('default_state', types['kFeature'].constants[1].mojom_name)
    self.assertEqual('true', types['kFeature'].constants[1].value)

  def testFeatureWeakKeyword(self):
    """Verifies that `feature` is a weak keyword."""
    types = self.ExtractTypes("""
      // e.g. BASE_DECLARE_FEATURE(kFeature);
      [AttributeOne=ValueOne]
      feature kFeature {
        // BASE_FEATURE(kFeature,"MyFeature",
        //     base::FEATURE_DISABLED_BY_DEFAULT);
        const string name = "MyFeature";
        const bool default_state = false;
      };
      struct MyStruct {
         bool feature = true;
      };
      interface InterfaceName {
         Method(string feature) => (int32 feature);
      };
    """)
    self.assertEqual('name', types['kFeature'].constants[0].mojom_name)
    self.assertEqual('"MyFeature"', types['kFeature'].constants[0].value)
    self.assertEqual('default_state', types['kFeature'].constants[1].mojom_name)
    self.assertEqual('false', types['kFeature'].constants[1].value)

  def testFeatureAttributesAreFeatures(self):
    """Verifies that feature values in attributes are really feature types."""
    a_mojom = 'a.mojom'
    self.WriteFile(
        a_mojom, 'module a;'
        'feature F { const string name = "f";'
        'const bool default_state = false; };')
    b_mojom = 'b.mojom'
    self.WriteFile(
        b_mojom, 'module b;'
        'import "a.mojom";'
        'feature G'
        '{const string name = "g"; const bool default_state = false;};'
        '[Attri=a.F] interface Foo { Foo(); };'
        '[Boink=G] interface Bar {};')
    self.ParseMojoms([a_mojom, b_mojom])
    b = self.LoadModule(b_mojom)
    self.assertEqual(b.interfaces[0].attributes['Attri'].mojom_name, 'F')
    self.assertEqual(b.interfaces[1].attributes['Boink'].mojom_name, 'G')

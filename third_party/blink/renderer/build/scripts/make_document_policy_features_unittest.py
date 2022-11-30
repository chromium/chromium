# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from make_document_policy_features_util import parse_default_value


class MakeDocumentPolicyFeaturesTest(unittest.TestCase):
    def test_parse_default_value(self):
        self.assertEqual(
            parse_default_value("max", "DecDouble"),
            "PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble)"
        )
        self.assertEqual(
            parse_default_value("min", "DecDouble"),
            "PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kDecDouble)"
        )
        self.assertEqual(parse_default_value("false", "Bool"),
                         "PolicyValue::CreateBool(false)")
        self.assertEqual(parse_default_value("0.5", "DecDouble"),
                         "PolicyValue::CreateDecDouble(0.5)")

        with self.assertRaises(ValueError):
            parse_default_value("max", "NotImplemented")


if __name__ == "__main__":
    unittest.main()

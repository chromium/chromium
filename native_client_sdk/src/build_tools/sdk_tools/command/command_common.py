# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def GetValidBundles(manifest, bundle_names):
  valid_bundles = [bundle.name for bundle in manifest.GetBundles()]
  valid_bundles = set(bundle_names) & set(valid_bundles)
  invalid_bundles = set(bundle_names) - valid_bundles
  return valid_bundles, invalid_bundles

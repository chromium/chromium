# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import logging

import command_common
import sdk_update_common

def Uninstall(install_dir, local_manifest, bundle_names):
  valid_bundles, invalid_bundles = \
      command_common.GetValidBundles(local_manifest, bundle_names)
  if invalid_bundles:
    logging.warn('Unknown bundle(s): %s\n' % (', '.join(invalid_bundles)))

  if not valid_bundles:
    logging.warn('No bundles to uninstall.')
    return

  for bundle_name in valid_bundles:
    logging.info('Removing %s' % (bundle_name,))
    bundle_dir = os.path.join(install_dir, bundle_name)
    try:
      sdk_update_common.RemoveDir(bundle_dir)
    except Exception as e:
      logging.error('Failed to remove directory \"%s\".  %s' % (bundle_dir, e))
      continue
    local_manifest.RemoveBundle(bundle_name)

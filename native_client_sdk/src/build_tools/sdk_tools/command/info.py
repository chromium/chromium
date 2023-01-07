# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import command_common
import logging
import manifest_util

def Info(manifest, bundle_names):
  valid_bundles, invalid_bundles = command_common.GetValidBundles(manifest,
                                                                  bundle_names)
  if invalid_bundles:
    logging.warn('Unknown bundle(s): %s\n' % (', '.join(invalid_bundles)))

  if not valid_bundles:
    logging.warn('No valid bundles given.')
    return

  for bundle_name in valid_bundles:
    bundle = manifest.GetBundle(bundle_name)

    print bundle.name
    for key in sorted(bundle.iterkeys()):
      value = bundle[key]
      if key == manifest_util.ARCHIVES_KEY:
        for archive in bundle.GetArchives():
          print '  Archive:'
          if archive:
            for archive_key in sorted(archive.iterkeys()):
              print '    %s: %s' % (archive_key, archive[archive_key])
      elif key not in (manifest_util.ARCHIVES_KEY, manifest_util.NAME_KEY):
        print '  %s: %s' % (key, value)
    print

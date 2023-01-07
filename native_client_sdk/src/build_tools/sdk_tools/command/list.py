# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def List(remote_manifest, local_manifest, display_revisions):
  any_bundles_need_update = False
  print 'Bundles:'
  print ' I: installed\n *: update available\n'
  for bundle in remote_manifest.GetBundles():
    local_bundle = local_manifest.GetBundle(bundle.name)
    needs_update = local_bundle and local_manifest.BundleNeedsUpdate(bundle)
    if needs_update:
      any_bundles_need_update = True

    _PrintBundle(local_bundle, bundle, needs_update, display_revisions)

  if not any_bundles_need_update:
    print '\nAll installed bundles are up to date.'

  local_only_bundles = set([b.name for b in local_manifest.GetBundles()])
  local_only_bundles -= set([b.name for b in remote_manifest.GetBundles()])
  if local_only_bundles:
    print '\nBundles installed locally that are not available remotely:'
    for bundle_name in local_only_bundles:
      local_bundle = local_manifest.GetBundle(bundle_name)
      _PrintBundle(local_bundle, None, False, display_revisions)


def _PrintBundle(local_bundle, bundle, needs_update, display_revisions):
  installed = local_bundle is not None
  # If bundle is None, there is no longer a remote bundle with this name.
  if bundle is None:
    bundle = local_bundle

  if display_revisions:
    if needs_update:
      revision = ' (r%s -> r%s)' % (local_bundle.revision, bundle.revision)
    else:
      revision = ' (r%s)' % (bundle.revision,)
  else:
    revision = ''

  print ('  %s%s %s (%s)%s' % (
    'I' if installed else ' ',
    '*' if needs_update else ' ',
    bundle.name,
    bundle.stability,
    revision))

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import download
from sdk_update_common import Error

def AddSource(config, url):
  try:
    download.UrlOpen(url)
  except Exception as e:
    raise Error('Not adding %s, unable to load URL.\n  %s' % (url, e))
  config.AddSource(url)


def RemoveSource(config, url):
  if url == 'all':
    config.RemoveAllSources()
  else:
    config.RemoveSource(url)


def ListSources(config):
  if config.sources:
    print 'Installed sources:'
    for s in config.sources:
      print '  ' + s
  else:
    print 'No external sources installed.'

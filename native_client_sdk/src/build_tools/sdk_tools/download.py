# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import sys
# when pylint runs the third_party module is the one from depot_tools
# pylint: disable=E0611
from third_party import fancy_urllib
import urllib2


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def UrlOpen(url):
  request = fancy_urllib.FancyRequest(url)
  ca_certs = os.path.join(SCRIPT_DIR, 'cacerts.txt')
  request.set_ssl_info(ca_certs=ca_certs)
  url_opener = urllib2.build_opener(
      fancy_urllib.FancyProxyHandler(),
      fancy_urllib.FancyRedirectHandler(),
      fancy_urllib.FancyHTTPSHandler())
  return url_opener.open(request)


def MakeProgressFunction(file_size):
  '''Returns a progress function based on a known file size'''
  # An inner function can only read nonlocal variables, not assign them. We can
  # work around this by using a list of one element.
  dots = [0]
  console = sys.stdout.isatty()
  def ShowKnownProgress(progress):
    if progress == 0:
      sys.stdout.write('|%s|  %s\n' % ('=' * 48, file_size))
    elif progress == -1:
      sys.stdout.write('\n')
    else:
      new_dots = progress * 50 / file_size - dots[0]
      dots[0] += new_dots
      if not console:
        sys.stdout.write('.' * new_dots)
      else:
        # use line rewrite capability
        sys.stdout.write(
            '\r%s  %s/%s kB' % ('.' * dots[0], progress//1000, file_size//1000))
    sys.stdout.flush()

  return ShowKnownProgress


def DownloadAndComputeHash(from_stream, to_stream=None, progress_func=None):
  '''Read from from-stream and generate sha1 and size info.

  Args:
    from_stream:   An input stream that supports read.
    to_stream:     [optional] the data is written to to_stream if it is
                   provided.
    progress_func: [optional] A function used to report download progress. If
                   provided, progress_func is called with progress=0 at the
                   beginning of the download, periodically with progress>0
                   (== number of bytes read so far) during the download, and
                   progress=-1 at the end or if the download was aborted.

  Return
    A tuple (sha1, size) where sha1 is a sha1-hash for the archive data and
    size is the size of the archive data in bytes.'''
  # Use a no-op progress function if none is specified.
  def progress_no_op(progress):
    pass
  if not progress_func:
    progress_func = progress_no_op

  sha1_hash = hashlib.sha1()
  size = 0
  try:
    progress_func(progress=0)
    while True:
      data = from_stream.read(32768)
      if not data:
        break
      sha1_hash.update(data)
      size += len(data)
      if to_stream:
        to_stream.write(data)
      progress_func(size)
  finally:
    progress_func(progress=-1)
  return sha1_hash.hexdigest(), size

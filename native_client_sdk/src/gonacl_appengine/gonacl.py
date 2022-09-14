# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import webapp2

application = webapp2.WSGIApplication([
  webapp2.Route('/', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developer.chrome.com/native-client'}),
  webapp2.Route('/fire', webapp2.RedirectHandler, defaults={
    '_uri': 'http://developer.chrome.com/native-client/cds2014'}),
  webapp2.Route('/magic', webapp2.RedirectHandler, defaults={
    '_uri':
    'https://flagxor-html5devconf2015.storage.googleapis.com/index.html'}),
  webapp2.Route('/reportissue', webapp2.RedirectHandler, defaults={
    '_uri': 'https://code.google.com/p/nativeclient/issues/entry'}),
], debug=True)

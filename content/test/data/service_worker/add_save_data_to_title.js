// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
    var value = event.request.headers.get('Save-Data');
    var blob = new Blob(['<title>save-data='+ value + '</title>']);
    var response = new Response(blob, {
        headers: { 'Content-Type': 'text/html' }
    });
    event.respondWith(response);
};

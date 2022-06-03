// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.oninstall = function(event){
    this.onfetch = function(fetchevent) {
        var headers = new Headers;
        headers.set('Content-Language', 'fi');
        headers.set('Content-Type', 'text/html; charset=UTF-8');
        var blob = new Blob(["This resource is gone. Gone, gone, gone."]);
        var response = new Response(blob, {
            status: 301,
            statusText: 'Moved Permanently',
            headers: headers
        });

        fetchevent.respondWith(response);
    };
};

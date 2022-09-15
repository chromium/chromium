// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

this.onfetch = function(event) {
    var CACHE_NAME = 'cache_name';
    var cache = undefined;
    var url = event.request.url;
    event.respondWith(
        caches.open(CACHE_NAME)
          .then(function(c) {
              cache = c;
              return cache.match(url);
            })
          .then(function(response) {
              if (response)
                return response;
              var cloned_response = undefined;
              return fetch(url)
                .then(function(res) {
                    cloned_response = res.clone();
                    return cache.put(url, res);
                  })
                .then(function() {
                    return cloned_response;
                  });
            }));
  };

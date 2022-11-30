// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service worker used in ClearSiteDataThrottleBrowserTest.

// Handle all resource requests.
self.addEventListener('fetch', function(event) {
  var url = new URL(event.request.url);

  // If this is a request for 'resource_from_sw', serve that resource
  // with a Clear-Site-Data header.
  if (url.pathname.match('resource_from_sw')) {
    event.respondWith(new Response(
        'Response content is not important, only the header is.', {
            'headers': { 'Clear-Site-Data': '"cookies"' }
        }));
    return;
  }

  // If this is a request for 'resource', let it through. It will be responded
  // to by the test server.
  if (url.pathname.match('resource'))
    return;

  // Otherwise, serve the default response - a simple HTML page that will
  // execute the following function:
  var response_script_body = function(url_search) {
    // Parse the origin[1234] |url| parameters.
    var origins = {};
    for (var i = 1; i <= 4; i++) {
      var origin_param_regex = new RegExp('origin' + i + '=([^&=?]+)');
      origins[i] = decodeURIComponent(url_search.match(origin_param_regex)[1]);
    }

    // Prepare the test cases.
    var resource_urls = [
        origins[1] + 'resource',
        origins[2] + 'resource_from_sw',
        origins[3] + 'resource_from_sw',
        origins[4] + 'resource',
        origins[1] + 'resource_from_sw',
        origins[2] + 'resource',
        origins[3] + 'resource_from_sw',
        origins[4] + 'another_resource_so_that_the_previous_one_isnt_reused',
    ];
    var header = encodeURIComponent('"cookies"');

    // Fetch all resources and report back to the C++ side by setting
    // the document title.
    var fetchResource = function(index) {
      var img = new Image();
      document.body.appendChild(img);

      img.onload = img.onerror = function() {
        if (index + 1 == resource_urls.length)
          document.title = "done";
        else
          fetchResource(index + 1);
      }

      img.src = resource_urls[index] + "?header=" + header;
    }
    fetchResource(0);
  }

  // Return the code of |response_script_body| as the response.
  event.respondWith(new Response(
    '<html><head></head><body><script>' +
    '(' + response_script_body.toString() + ')("' + url.search + '")' +
    '</script></body></html>',
    { 'headers': { 'Content-Type': 'text/html' } }
  ));
});

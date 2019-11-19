// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cookies are matched up with resources correctly.\n`);


  var cookies = [
    createCookie('insecureOnlyWebkit', '1234567890', false, 'webkit.org', '/'),
    createCookie('insecureAllWebkit', '1234567890123456', false, '.webkit.org', '/'),
    createCookie('insecureAllWebkitPath', '1234567890123456', false, '.webkit.org', '/path'),
    createCookie('secureOnlyWebkitPath', 'bar', true, 'webkit.org', '/path'),
    createCookie('secureAllWebkit', 'foo', true, '.webkit.org', '/'),
    createCookie('secureAllWebkitPath', 'foo', true, '.webkit.org', '/path'),
    createCookie('insecureOnlyWebkitPort', '1234567890', false, 'webkit.org', '/', '80'),
    createCookie('insecureAllWebkitPort', '1234567890123456', false, '.webkit.org', '/', '80'),
    createCookie('insecureAllWebkitPathPort', '1234567890123456', false, '.webkit.org', '/path', '80'),
    createCookie('secureOnlyWebkitPathPort', 'bar', true, 'webkit.org', '/path', '80'),
    createCookie('secureAllWebkitPort', 'foo', true, '.webkit.org', '/', '80'),
    createCookie('secureAllWebkitPathPort', 'foo', true, '.webkit.org', '/path', '80'),
    createCookie('nonMatching1', 'bar', false, 'webkit.zoo', '/'),
    createCookie('nonMatching2', 'bar', false, 'webkit.org', '/badPath'),
    createCookie('nonMatching3', 'bar', true, '.moo.com', '/')
  ];

  var resourceURLs = [
    'http://webkit.org',            // 0
    'http://www.webkit.org:81',     // 1
    'http://webkit.org/path',       // 2
    'http://www.webkit.org/path',   // 3
    'https://webkit.org/',          // 4
    'https://www.webkit.org/',      // 5
    'https://webkit.org:81/path',   // 6
    'https://www.webkit.org/path',  // 7

    'http://webkit.org:80',            // 8
    'http://www.webkit.org:80',        // 9
    'http://webkit.org:80/path',       // 10
    'http://www.webkit.org:80/path',   // 11
    'https://webkit.org:80/',          // 12
    'https://www.webkit.org:80/',      // 13
    'https://webkit.org:80/path',      // 14
    'https://www.webkit.org:80/path',  // 15

    'http://www.boo.com:80',        // 16
    'https://www.boo.com:80/path',  // 17
    'http://www.moo.com:80/',       // 18
    'http://www.boo.com:80',        // 19
    'https://www.boo.com:80/path',  // 20
    'http://www.moo.com:80/'        // 21
  ];

  var result = [];
  for (var i = 0; i < cookies.length; ++i) {
    var cookieResult = [];
    for (var j = 0; j < resourceURLs.length; ++j) {
      if (SDK.CookieModel.cookieMatchesResourceURL(cookies[i], resourceURLs[j]))
        cookieResult.push(j);
    }
    TestRunner.addResult('[' + cookieResult + ']');
  }
  TestRunner.completeTest();

  function createCookie(name, value, secure, domain, path, port) {
    var protocolCookie = {
      name: name,
      value: value,
      domain: domain,
      port: port,
      path: path,
      expires: 'Thu Jan 01 1970 00:00:00 GMT',
      size: name.length + value.length,
      httpOnly: false,
      secure: secure,
      session: true
    };
    var target = SDK.targetManager.mainTarget();
    return SDK.Cookie.fromProtocolCookie(protocolCookie);
  }
})();

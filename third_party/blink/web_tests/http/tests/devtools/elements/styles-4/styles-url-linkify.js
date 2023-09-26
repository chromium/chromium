// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(
      `Tests that URLs are linked to and completed correctly. Bugs 51663, 53171, 62643, 72373, 79905\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="local"></div>
      <iframe src="../styles/resources/styles-url-linkify-iframe.html"></iframe>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/styles-url-linkify.css');

  function completeURL(baseURL, href) {
    TestRunner.addResult(Common.ParsedURL.ParsedURL.completeURL(baseURL, href));
  }

  TestRunner.addResult('URLs completed:');
  completeURL('http://example.com', '/');
  completeURL('http://example.com', 'moo');
  completeURL('http://example.com/', 'https://secure.com/moo');
  completeURL('https://example.com/foo', '//secure.com/moo');
  completeURL('http://example.com/foo/zoo', '/moo');
  completeURL('http://example.com/foo/zoo/', 'moo');
  completeURL('http://example.com/foo/zoo', 'boo/moo');
  completeURL('http://example.com/foo', 'moo');
  completeURL('http://example.com/foo', '?a=b');
  completeURL('http://example.com/foo', '?a=/b');
  completeURL('http://example.com/?c=/d#anchor', '?a=/b');
  completeURL('http://example.com/foo?c=d', '?a=b');
  completeURL('http://example.com/foo?c=d#anchor', '?a=/b');
  completeURL('http://example.com/foo?c=/d/e', '?a=b');
  completeURL('http://example.com/foo?c=/d/e', 'cat.jpeg');
  completeURL('http://example.com/foo#anchor', 'cat.jpeg');
  completeURL('http://example.com/', '/foo?bar=http://otherexample.com');

  const dataURL =
      'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEIAAABCAgMAAACeOuh7AAAABGdBTUEAAK/INwWK6QAAAAlQTFRF////AAAA////fu+PTwAAAAF0Uk5TAEDm2GYAAACHSURBVDjLxdLbDYAgDAVQGELn0R3oEHYf2KGdUqtE46OFRCP3oyTng1xCnWsaD5JRRtCkQ2YmkBkHRXqWJBn0j0TICbrsWVoWhRShCdcGyZCtHxMaUnVPRZ9KSbmBJdsX2vJVnwqRD0Rb4rpzgIbE/AI5NTnWAMvy5l0dXrfuLh5OCe5BmmYGXhTUxlQ5xJ8AAAAASUVORK5CYII=';
  const blobURL = 'blob:http://example.com/f91b7b00-00-0000-0000-3b7c87055d7a';
  completeURL('https://example.com/foo', dataURL);
  completeURL('http://example.com/foo', 'javascript:alert(\'foo\');');
  completeURL('http://example.com/foo', blobURL);
  completeURL('', blobURL);

  function dumpHref(dumpLinkClass) {
    var hrefNode;
    var valueChildNodes =
        ElementsTestRunner.firstMatchedStyleSection().propertiesTreeOutline.firstChild().valueElement.childNodes;
    for (var i = 0; i < valueChildNodes.length; ++i) {
      if (valueChildNodes[i].href) {
        hrefNode = valueChildNodes[i];
        break;
      }
    }
    if (!hrefNode) {
      TestRunner.addResult('href property not found');
      return;
    }

    var styleClass = '';
    if (dumpLinkClass) {
      if (hrefNode.classList.contains('devtools-link'))
        styleClass += 'devtools-link ';
    }

    var href = hrefNode.href;
    var segments = href.split('/');
    var output = [];
    for (var i = segments.length - 1, minSegment = i - 3; i >= 0 && i >= minSegment; --i)
      output.unshift(segments[i]);

    TestRunner.addResult(styleClass + output.join('/'));
  }

  ElementsTestRunner.selectNodeAndWaitForStyles('local', step1);

  function step1() {
    TestRunner.addResult('Link for a URI from CSS document:');
    dumpHref(true);
    ElementsTestRunner.selectNodeAndWaitForStyles('iframed', step2);
  }

  function step2() {
    TestRunner.addResult('Link for a URI from iframe inline stylesheet:');
    dumpHref();
    TestRunner.completeTest();
  }
})();

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that elements panel search is returning proper results.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
      <html id="documentElement">
      <head><base href=${TestRunner.url()}></head>
      <body>
      <div>FooBar</div>
      <input value="InputVal">
      <div attr="foo"></div>
      <div id="terminator"></div>
      <div class="divclass"><span>Found by selector</span></div>
      <span class="foo koo"></span>
      <span class="CASELESS"></span>
      <span data-camel="insenstive"></span>
      <div id="shadow-host">
          <div id="shadow-host-content"></div>
      </div>
      <template id="shadow-dom-template">
        <div id="shadow-dom-outer">
            <content></content>
        </div>

      </template>
      <textarea></textarea>
      <div id="ua-shadow-host"></div>
      </body>
      </html>
    `);
  await TestRunner.evaluateInPagePromise(`
      function initializeShadowDOM()
      {
          var shadow = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
          var template = document.querySelector('#shadow-dom-template');

          // Avoid matching this function
          shadow.appendChild(template.content.cloneNode(true));

          var uaShadow = internals.createUserAgentShadowRoot(
              document.querySelector('#ua-shadow-host'));
          var uaShadowContent = document.createElement('div');
          uaShadowContent.id = 'ua-shadow-' + 'content';
          uaShadow.appendChild(uaShadowContent);
      }
  `);

  var omitInnerHTML;

  async function searchCallback(next, resultCount) {
    if (resultCount == 0) {
      TestRunner.addResult('Nothing found');
      SDK.DOMModel.DOMModel.cancelSearch();
      next();
      return;
    }

    for (var i = 0; i < resultCount; ++i) {
      var node = await TestRunner.domModel.searchResult(i);
      var markupVa_lue = await node.getOuterHTML();
      if (omitInnerHTML)
        markupVa_lue = markupVa_lue.substr(0, markupVa_lue.indexOf('>') + 1);
      TestRunner.addResult(markupVa_lue.split('').join(' '));
    }

    SDK.DOMModel.DOMModel.cancelSearch();
    next();
  }

  function setUp(next) {
    TestRunner.domModel.requestDocument().then(step2);

    function step2() {
      TestRunner.evaluateInPage('initializeShadowDOM()', next);
    }
  }

  TestRunner.runTestSuite([
    function testSetUp(next) {
      setUp(next);
    },

    function testPlainText(next) {
      TestRunner.domModel
          .performSearch(
              'Fo' +
                  'o' +
                  'Bar',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testPartialText(next) {
      TestRunner.domModel
          .performSearch(
              'oo' +
                  'Ba',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testStartTag(next) {
      TestRunner.domModel
          .performSearch(
              '<inpu' +
                  't',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testEndTag(next) {
      TestRunner.domModel
          .performSearch(
              'npu' +
                  't>',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testPartialTag(next) {
      TestRunner.domModel
          .performSearch(
              'npu' +
                  't',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testPartialAbsentTagStart(next) {
      TestRunner.domModel
          .performSearch(
              '<npu' +
                  't',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testPartialAbsentTagEnd(next) {
      TestRunner.domModel
          .performSearch(
              'npu' +
                  '>',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testFullTag(next) {
      TestRunner.domModel
          .performSearch(
              '<inpu' +
                  't>',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testExactAttributeName(next) {
      TestRunner.domModel
          .performSearch(
              'valu' +
                  'e',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testExactAttributeVal_ue(next) {
      TestRunner.domModel
          .performSearch(
              'In' +
                  'putVa' +
                  'l',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testExactAttributeVal_ueOnRoot(next) {
      omitInnerHTML = true;
      TestRunner.domModel
          .performSearch(
              'documen' +
                  'tElement',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testExactAttributeVal_ueWithQuotes(next) {
      omitInnerHTML = false;
      TestRunner.domModel
          .performSearch(
              '"fo' +
                  'o"',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testPartialAttributeVal_ue(next) {
      TestRunner.domModel
          .performSearch(
              'n' +
                  'putVa' +
                  'l',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testXPathAttribute(next) {
      TestRunner.domModel
          .performSearch(
              '//html' +
                  '//@attr',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testSelector(next) {
      TestRunner.domModel
          .performSearch(
              'd' +
                  'iv.divclass span',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testCaseUpperFindsLower(next) {
      TestRunner.domModel
          .performSearch(
              'K' +
                  'OO',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testCaseLowerFindsUpper(next) {
      TestRunner.domModel
          .performSearch(
              'c' +
                  'aseless',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testCaseAttribute(next) {
      TestRunner.domModel
          .performSearch(
              'C' +
                  'AMEL',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testSearchShadowDOM(next) {
      TestRunner.domModel
          .performSearch(
              '<c' +
                  'ontent',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testSearchUAShadowDOM(next) {
      TestRunner.addResult('Searching UA shadow DOM with setting disabled:');
      TestRunner.domModel
          .performSearch(
              'ua-shadow-' +
                  'content',
              false)
          .then(searchCallback.bind(this, step2));

      function step2() {
        TestRunner.addResult('Searching UA shadow DOM with setting enabled:');
        TestRunner.domModel
            .performSearch(
                'ua-shadow-' +
                    'content',
                true)
            .then(searchCallback.bind(this, next));
      }
    },

    function testSearchShadowHostChildren(next) {
      TestRunner.domModel
          .performSearch(
              'shadow-host-c' +
                  'ontent',
              false)
          .then(searchCallback.bind(this, next));
    },

    function testSearchClosingTag(next) {
        TestRunner.domModel
            .performSearch('</textarea>', false)
            .then(searchCallback.bind(this, next));
      },
  ]);
})();

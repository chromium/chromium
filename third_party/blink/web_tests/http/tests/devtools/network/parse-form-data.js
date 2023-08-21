// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  function getHTMLWithEncType(values, encType) {
    const encTypeAttribute = encType ? ` enctype="${encType}"` : '';

    const inputElements = values.map(
      ({element = 'input', name, value, content = ''}) => {
        const valueAttribute = value ? ` value="${value}"` : '';
        return `<${element} type="text" name="${name}"${valueAttribute}>${content}</${element}>`;
      }).join();
    return `
      <form action="" method="post"${encTypeAttribute}>
        ${inputElements}
      </form>`;
  }

  async function runFormTest(values, encType) {
    await TestRunner.loadHTML(getHTMLWithEncType(values, encType));
    const snifferPromise = TestRunner.addSnifferPromise(SDK.NetworkManager.NetworkDispatcher.prototype, 'requestWillBeSent');
    TestRunner.evaluateInPage('document.querySelector("form").submit();');
    await snifferPromise;

    const networkRequests = NetworkTestRunner.networkRequests();
    var request = networkRequests[networkRequests.length - 1];
    if (request.url().endsWith('/')) {
      await TestRunner.addSnifferPromise(SDK.NetworkManager.NetworkDispatcher.prototype, 'requestWillBeSent');
      request = NetworkTestRunner.networkRequests().pop();
    }

    const formData = await request.formParameters();

    TestRunner.addResult('resource.requestContentType: ' + request.requestContentType().replace(/; (boundary)=.+/, '; $1=$1'));
    TestRunner.addResult(JSON.stringify(formData, ' ', 1));
  }

  const formValues = [
    {name: 'foo1', value: 'bar1'},
    {name: 'foo2', value: 'bar2'},
    {name: 'foo3', value: 'bar3'},
    {name: 'foo4', value: 'bar4'},
    {name: 'foo5', value: 'bar5'},
    {element: 'textarea', name: 'foo6', content: 'ba&#13;r6'},
    {element: 'textarea', name: 'foo7', content: 'ba&#10;r7'},
    {element: 'textarea', name: 'foo8', content: 'ba&#10;&#13;r8'},
    {element: 'textarea', name: 'foo9', content: 'ba&#13;&#10;r9'},
    {element: 'textarea', name: 'fo&#13;o10', content: 'ba&#13;&#10;r10'},
    {element: 'textarea', name: 'fo&#10;o11', content: 'ba&#13;&#10;r10'},
    {element: 'textarea', name: 'fo&#10;&#13;o11', content: 'ba&#13;&#10;r10'},
    {element: 'textarea', name: 'fo&#13;&#10;o11', content: 'ba&#13;&#10;r10'},
    {element: 'textarea', name: 'f&quot;o&#13;&#10;o11', content: 'ba&#13;&#10;r10'},
    {element: 'textarea', name: 'f&quot;o&#13;&#10;o11', content: 'ba&#13;&#10;&quot;r10'}
  ];

  TestRunner.addResult('Tests that form submissions appear and are parsed in the network panel');
  await TestRunner.showPanel('network');

  await runFormTest(formValues);
  await runFormTest(formValues, 'multipart/form-data');

  TestRunner.addResult('resource.requestContentType: multipart/form-data with non-URL-encoded field names and a file');
  const newBoundary = '---------------------------7e21f12b350cf2';
  const nonURLEncodedNameRequestBody =
    `--${newBoundary}\r\nContent-Disposition: form-data; name=\"a\r\nb\"\r\n\r\na\r\nv\r\n` +
    `--${newBoundary}\r\nContent-Disposition: form-data; name=\"a\r\nc\"; filename="a.gif"\r\nContent-Type: application/octer-stream\r\n\r\na\r\nv\r\n` +
    `--${newBoundary}--\r\n\u0000`;
  const nonURLEncodedNameFormData = SDK.NetworkRequest.NetworkRequest.prototype.parseMultipartFormDataParameters(nonURLEncodedNameRequestBody, newBoundary);

  TestRunner.addResult(JSON.stringify(nonURLEncodedNameFormData, ' ', 1));

  TestRunner.completeTest();
})();

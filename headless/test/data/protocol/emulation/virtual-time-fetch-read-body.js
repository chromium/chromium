// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests that ' +
      'Virtual Time is paused while reading fetch response body at once');

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`<html></html>`)
  );
  helper.onceRequest('http://test.com/fetch/array-buffer').fulfill(
      FetchHelper.makeResponse('\x2a')
  );
  helper.onceRequest('http://test.com/fetch/blob').fulfill(
      FetchHelper.makeResponse('blob')
  );
  helper.onceRequest('http://test.com/fetch/form-data').fulfill(
      FetchHelper.makeResponse('key=value&key2=value2',
           ['Content-type: application/x-www-form-urlencoded'])
  );
  helper.onceRequest('http://test.com/fetch/json').fulfill(
      FetchHelper.makeResponse('{"key": "value"}')
  );
  helper.onceRequest('http://test.com/fetch/text').fulfill(
      FetchHelper.makeResponse('text')
  );

  dp.Runtime.enable();
  // Defer logging of console messages so these do not intervine with
  // interception-side logging.
  const console_messages = [];
  dp.Runtime.onConsoleAPICalled(({params}) => {
    console_messages.push(params.args[0].value);
  });

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  session.evaluate(`(async function() {
    console.log('started');
    const array_buf = await fetch('/fetch/array-buffer')
                          .then(r => r.arrayBuffer());
    console.log('got array buffer: ' + new DataView(array_buf).getInt8(0));
    const blob = await fetch('/fetch/blob').then(r => r.blob());
    console.log('got blob');
    const form_data = await fetch('/fetch/form-data')
                          .then(r => r.formData());
    console.log('got form data: ' + Array.from(form_data)
        .map(([a, b]) => '{' + a + ':' + b + '}').join(','));
    const value = await fetch('/fetch/json')
                      .then(r => r.json());
    console.log('got json: ' + JSON.stringify(value));
    const text = await fetch('/fetch/text')
                     .then(r => r.text());
    console.log('got text: ' + text);
    console.log('PASS: all done');
  })()`);
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending', budget: 5000});

  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.log(console_messages);
  testRunner.completeTest();
})

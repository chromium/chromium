// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that virtual time works with storage.`);

  const FetchHelper = await testRunner.loadScriptAbsolute(
      '../fetch/resources/fetch-test.js');
  const helper = new FetchHelper(testRunner, dp);
  await helper.enable();

  helper.onceRequest('http://test.com/index.html').fulfill(
      FetchHelper.makeContentResponse(`
          <html>
          <body>
          <script>
            function createFrame(url, id) {
              const frame = document.createElement('iframe');
              frame.id = id;
              frame.src = url;
              return new Promise(resolve => {
                frame.onload = resolve;
                document.body.appendChild(frame);
              })
            }
            window.addEventListener('message', () => {
              document.getElementById('frame1').remove();
              document.getElementById('frame2').contentWindow.postMessage('doit', 'http://test2.com');
            });
            Promise.all([
              createFrame('http://test2.com/frame1.html', 'frame1'),
              createFrame('http://test2.com/frame2.html', 'frame2')
            ]).then(() => {
              frames[0].postMessage('doit', 'http://test2.com');
            });
          </script>
          </body>
          </html>`)
  );

  const frameSource = `
    <html><body><script>
    window.addEventListener('message', async (event) => {
      localStorage.setItem('foo', 'bar');
      event.source.postMessage('done', event.origin);
    });
    </script></body></html>`;
  helper.onceRequest('http://test2.com/frame1.html').fulfill(
    FetchHelper.makeContentResponse(frameSource)
  );
  helper.onceRequest('http://test2.com/frame2.html').fulfill(
    FetchHelper.makeContentResponse(frameSource)
  );

  await dp.Emulation.setVirtualTimePolicy({policy: 'pause'});
  await dp.Page.navigate({url: 'http://test.com/index.html'});
  dp.Emulation.setVirtualTimePolicy({
    policy: 'pauseIfNetworkFetchesPending',
    budget: 5000,
    maxVirtualTimeTaskStarvationCount: 10000});
  await dp.Emulation.onceVirtualTimeBudgetExpired();
  testRunner.completeTest();
})

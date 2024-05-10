// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `
      <html>
      <body>
        <div id="fullscreen-div">The element.</div>
      </body>
      <script>
        function toggleFullscreen() {
          const element = document.getElementById("fullscreen-div");
          if (!document.fullscreenElement) {
            element.requestFullscreen();
          } else {
            document.exitFullscreen();
          }
        }

        function fullscreenchanged(event) {
          if (document.fullscreenElement) {
            console.log("Entered fullscreen mode: "
                + document.fullscreenElement.id);
          }
        }

        document.getElementById("fullscreen-div")
          .addEventListener("fullscreenchange", fullscreenchanged);
      </script>
      </html>
  `;
  const {session, dp} =
      await testRunner.startHTML(html, 'Tests element requestFullscreen.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
  });

  await dp.Page.enable();

  session.evaluateAsyncWithUserGesture('window.toggleFullscreen();');
  await dp.Page.onceFrameResized();

  session.evaluateAsyncWithUserGesture('window.toggleFullscreen();');
  await dp.Page.onceFrameResized();

  testRunner.completeTest();
})

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function(testRunner) {
  const html = `
      <html>
      <style>
        div {
            font: 50px/1 monospace;
            width: 4ch;
            hyphens: auto;
        }
      </style>

      <body lang="en">
        <div id="hyphenated-text">reallylongwordwithnobreaks</div>
      </body>

      <script>
        function getHyphenatedTextSize() {
          const element = document.getElementById("hyphenated-text");
          return {width: element.offsetWidth, height: element.offsetHeight};
        }
      </script>
    </html>
  `;
  const {session} =
      await testRunner.startHTML(html, 'Tests text auto hyphenation.');

  const {width, height} =
      await session.evaluate('window.getHyphenatedTextSize();');

  if (height > width) {
    testRunner.log('PASS');
  } else {
    testRunner.log(`FAIL: text size: ${width}x${height}`);
  }

  testRunner.completeTest();
})

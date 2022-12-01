// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<input type="file" id="file" webkitdirectory multiple>`,
      'Tests DOM.setFileInfo method.');

  const dataPath = testRunner.params("data_path");
  const {result} = await dp.Runtime.evaluate({
     expression: `document.getElementById('file')`
  });
  dp.DOM.setFileInputFiles({
    objectId: result.result.objectId,
    files: [dataPath]
  });
  const value = await session.evaluateAsync(`new Promise(resolve => {
    const file = document.getElementById('file');
    async function readFile(f) {
      return f.name + ': ' + await f.text() + "------------------";
    }
    file.addEventListener('input', async () => {
      const contents = await Promise.all(Array.from(file.files).map(readFile));
      resolve(contents.join('\\n'));
    });
  })`);
  testRunner.log(value);
  testRunner.completeTest();
})

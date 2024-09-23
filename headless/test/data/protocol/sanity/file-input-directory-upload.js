// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<input type="file" id="file" webkitdirectory multiple>`,
      'Tests DOM.setFileInputFiles method.');

  const dataPath = testRunner.params("data_path");
  const {result} = await dp.Runtime.evaluate({
     expression: `document.getElementById('file')`
  });
  const valuePromise = session.evaluateAsync(`new Promise(resolve => {
    const file = document.getElementById('file');
    async function readFile(f) {
      const parts = [
        'name: ' + f.name,
        'webkitRelativePath: ' + f.webkitRelativePath,
        'content: ' + await f.text(),
      ];
      return parts.join(', ') + "------------------";
    }
    file.addEventListener('input', async () => {
      const contents = await Promise.all(Array.from(file.files)
        .sort((a, b) => a.name.localeCompare(b.name)).map(readFile));
      resolve(contents.join('\\n'));
    });
  })`);
  dp.DOM.setFileInputFiles({
    objectId: result.result.objectId,
    files: [dataPath]
  });
  testRunner.log(await valuePromise);
  testRunner.completeTest();
})

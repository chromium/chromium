// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `This tests that ChunkedFileReader properly re-assembles chunks, especially in case these contain multibyte characters.\n`);


  var text = [
    'Латынь из моды вышла ныне:\n', 'Так, если правду вам сказать,\n', 'Он знал довольно по-латыне,\n',
    'Чтоб эпиграфы разбирать\n'
  ];

  var blob = new Blob(text);
  // Most of the characters above will be encoded as 2 bytes, so make sure we use odd
  // chunk size to cause chunk boundaries sometimes to happen between chaacter bytes.
  var chunkSize = 5;
  var chunkCount = 0;
  var reader = new Bindings.ChunkedFileReader(blob, chunkSize, () => ++chunkCount);
  var output = new Common.StringOutputStream();

  var error = await reader.read(output);

  TestRunner.addResult('Read result: ' + error);
  TestRunner.addResult('Chunks transferred: ' + chunkCount);
  TestRunner.assertEquals(text.join(''), output.data(), 'Read text is different from written text');
  TestRunner.addResult('DONE');
  TestRunner.completeTest();
})();

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

onmessage = e => {
  let result = 'Success';

  switch(e.data) {
    case 'tryAccessStorageFoundationSync':
      try {
        storageFoundation.getAllSync();
        result = 'Failure';
      } catch (error) {}
      try {
        let file = storageFoundation.openSync('foo');
        file.close();
        result = 'Failure';
      } catch (error) {}
      try {
        storageFoundation.renameSync('foo', 'bar');
        result = 'Failure';
      } catch (error) {}
      try {
        storageFoundation.deleteSync('bar');
        result = 'Failure';
      } catch (error) {}
      break;
    default:
      result = 'unknown message received';
  }

  postMessage(result);
}

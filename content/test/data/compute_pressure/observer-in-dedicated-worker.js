// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function pressureChange(data) {
  postMessage({type: 'data', update: data[0].toJSON()});
}

const observer = new PressureObserver(pressureChange);

onmessage = async (e) => {
  if (e.data.command == 'start') {
    try {
      await observer.observe('cpu');
      postMessage({result: 'success'});
    } catch (error) {
      postMessage({result: 'fail'});
    }
  }
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let transport;

onmessage = async (msg) => {
  const tokens = msg.data.split(',');
  switch (tokens[0]) {
  case 'open':
    const port = tokens[1];
    transport = new WebTransport('https://localhost:' + port + '/echo');
    await transport.ready;
    postMessage('opened');
    break;
  case 'close':
    transport.close();
    await transport.closed;
    postMessage('closed');
    break;
  default:
    console.error(msg);
    break;
  }
};

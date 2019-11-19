// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const worker = new Worker("./worker.js");

// This worker is a simple bidirectional proxy.
onmessage = message => worker.postMessage(message.data);
worker.onmessage = message => postMessage(message.data);

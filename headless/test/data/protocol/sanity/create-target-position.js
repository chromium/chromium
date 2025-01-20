// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests Target.createTarget() window positioning.');

  const {targetId} = (await session.protocol.Target.createTarget({
                       'url': 'about:blank',
                       'left': 10,
                       'top': 20,
                       'width': 400,
                       'height': 300
                     })).result;

  const {bounds} = (await dp.Browser.getWindowForTarget({targetId})).result;
  testRunner.log(bounds, 'Window bounds: ');

  testRunner.completeTest();
})

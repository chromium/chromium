// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Input.dispatchMouseEvent keeps OOPIF wheel deltas and scroll distances in CSS pixels with embedded CSS zoom and parent pinch.');
  const armWheelProbe =
      await testRunner.loadScript('../resources/wheel-event-helper.js');
  const deltaTolerance = 0.0001;
  const scrollTolerance = 0.5;
  const scaleTolerance = 0.000001;

  function check(condition, label, actual) {
    testRunner.log(`${condition ? 'PASS' : 'FAIL'}: ${label}`);
    if (!condition)
      testRunner.log(actual);
    return condition;
  }

  function near(actual, expected, tolerance) {
    return Number.isFinite(actual) && Math.abs(actual - expected) <= tolerance;
  }

  function protocolSucceeded(response, label) {
    return !response.error ||
        check(false, `${label}: protocol error`, response.error);
  }

  async function settle(renderer) {
    await renderer.evaluateAsync(
        () => new Promise(resolve => requestAnimationFrame(
                              () => requestAnimationFrame(resolve))));
  }

  // This host is isolated by the web-test browser client. Dispatch through its
  // iframe Target, so the child view's CSS zoom participates in ScaleFactor().
  const attached = dp.Target.onceAttachedToTarget();
  const response = await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  if (!protocolSucceeded(response, 'auto attach'))
    return testRunner.completeTest();
  const loaded = session.evaluateAsync(
      () => new Promise(resolve => {
        document.documentElement.style.overflow = 'hidden';
        document.body.style.margin = '0';
        internals.setPageScaleFactorLimits(1, 2);
        const iframe = document.createElement('iframe');
        iframe.id = 'child';
        iframe.style.cssText = 'border:0;width:240px;height:240px';
        iframe.onload = () => resolve();
        iframe.src =
            'http://devtools.oopif.test:8000/inspector-protocol/resources/iframe.html';
        document.body.appendChild(iframe);
      }));
  const {sessionId, targetInfo} = (await attached).params;
  if (targetInfo.type !== 'iframe') {
    check(false, 'isolated iframe Target attached', targetInfo);
    return testRunner.completeTest();
  }
  const child = session.createChild(sessionId);
  const childDp = child.protocol;
  await loaded;
  const initialChild =
      await child.evaluate(() => ({url: location.href, dpr: devicePixelRatio}));
  if (!check(initialChild.url.startsWith('http://devtools.oopif.test:8000/') &&
                 Number.isFinite(initialChild.dpr) && initialChild.dpr > 0,
             'isolated iframe Target attached', initialChild)) {
    return testRunner.completeTest();
  }

  async function resetScroller() {
    await child.evaluate(() => {
      document.documentElement.style.overflow = 'hidden';
      document.body.style.margin = '0';
      document.body.innerHTML = `
          <div id="scroller" style="width:200px;height:200px;overflow:scroll">
            <div id="content" style="width:2000px;height:2000px"></div>
          </div>`;
    });
    await settle(child);
  }

  async function captureWheel(deltaX, deltaY, cancel, label, scale) {
    await child.evaluate(armWheelProbe, cancel);
    const response = await childDp.Input.dispatchMouseEvent(
        {type: 'mouseWheel', x: 15, y: 15, deltaX, deltaY});
    if (!protocolSucceeded(response, label)) {
      await child.evaluate(() => window.finalizeWheelProbe());
      return null;
    }
    let probe = await child.evaluateAsync(() => window.wheelProbe);
    if (!check(!probe.timedOut && probe.wheelReceived && probe.scrollEnded,
               `${label}: expected events completed`, probe)) {
      return null;
    }
    if (cancel) {
      // Read current offsets and all events after dispatch and renderer frames,
      // rather than the snapshot taken inside the first wheel listener.
      await settle(child);
      probe = await child.evaluate(() => window.finalizeWheelProbe());
    }
    const record = probe.records[0];
    check(probe.records.length === 1 && record.target === 'content' &&
              record.deltaMode === 0 && record.cancelled === cancel &&
              (scale !== 1 ||
               (near(record.clientX, 15, deltaTolerance) &&
                near(record.clientY, 15, deltaTolerance))) &&
              near(record.deltaX, deltaX, deltaTolerance) &&
              near(record.deltaY, deltaY, deltaTolerance),
          `${label}: wheel deltas in child CSS pixels`, probe.records);
    return probe;
  }

  // Check child-session coordinates without pinch. Parent pinch has a
  // separate existing coordinate mismatch, so those cases check only DOM
  // deltas, default scrolling and cancellation with a broad content target.
  for (const zoom of [1, 1.2, 1.5]) {
    await session.evaluate(zoom => {
      document.getElementById('child').style.zoom = zoom;
    }, zoom);
    for (const scale of [1, 1.2]) {
      const label = `CSS zoom ${zoom} page scale ${scale}`;
      await resetScroller();
      const response =
          await dp.Emulation.setPageScaleFactor({pageScaleFactor: scale});
      if (!protocolSucceeded(response, label))
        return testRunner.completeTest();
      await settle(session);
      await settle(child);
      const geometry = await session.evaluate(() => {
        const iframe = document.getElementById('child');
        return {
          zoom: Number(getComputedStyle(iframe).zoom),
          width: iframe.getBoundingClientRect().width,
          height: iframe.getBoundingClientRect().height,
          pageScale: visualViewport.scale,
        };
      });
      // Child DPR includes its layout zoom, so this also admits that the child
      // renderer received the parent's new embedded zoom before dispatch.
      geometry.childDpr = await child.evaluate(() => devicePixelRatio);
      if (!check(near(geometry.zoom, zoom, scaleTolerance) &&
                     near(geometry.width / 240, zoom, scaleTolerance) &&
                     near(geometry.height / 240, zoom, scaleTolerance) &&
                     near(geometry.childDpr / initialChild.dpr, zoom,
                          scaleTolerance) &&
                     near(geometry.pageScale, scale, scaleTolerance),
                 `${label}: actual embedded zoom and page scale`, geometry)) {
        return testRunner.completeTest();
      }

      let expectedX = 0;
      let expectedY = 0;
      for (const {name,
                  deltaX,
                  deltaY} of [{name: 'forward', deltaX: 55, deltaY: 70},
                              {name: 'reverse', deltaX: -6, deltaY: -10},
      ]) {
        const probe = await captureWheel(deltaX, deltaY, false,
                                         `${label} ${name}`, scale);
        if (!probe)
          return testRunner.completeTest();
        expectedX += deltaX;
        expectedY += deltaY;
        check(near(probe.after[0], expectedX, scrollTolerance) &&
                  near(probe.after[1], expectedY, scrollTolerance),
              `${label} ${name}: scroll distance in child CSS pixels`,
              probe.after);
      }

      const probe =
          await captureWheel(0.1, -0.2, true, `${label} fractional`, scale);
      if (!probe)
        return testRunner.completeTest();
      check(probe.after[0] === probe.before[0] &&
                probe.after[1] === probe.before[1],
            `${label} fractional: cancelled wheel leaves offset unchanged`,
            probe);
    }
  }

  const restored = await dp.Emulation.setPageScaleFactor({pageScaleFactor: 1});
  await session.evaluate(() => document.getElementById('child').style.zoom = 1);
  await settle(session);
  await settle(child);
  if (protocolSucceeded(restored, 'restore scale')) {
    check(
        near(await session.evaluate(() => visualViewport.scale), 1,
             scaleTolerance) &&
            await session.evaluate(
                () => Number(getComputedStyle(document.getElementById('child'))
                                 .zoom)) === 1 &&
            near((await child.evaluate(() => devicePixelRatio)) /
                     initialChild.dpr,
                 1, scaleTolerance),
        'restored page scale and embedded CSS zoom', null);
  }
  await child.disconnect();
  testRunner.completeTest();
})

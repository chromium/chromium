(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<html><body><iframe id='frame'></iframe></body></html>`,
      'Tests Page.frameStartedLoading / Page.frameStoppedLoading events with ' +
      ' different kinds of frame navigation');

  function setLogFrameLoadingEvents(dp, prefix) {
    dp.Page.enable();
    dp.Page.setLifecycleEventsEnabled({enabled: true});
    dp.Page.onceFrameStartedLoading(e => testRunner.log(`${prefix}: ${e.method}`));
    dp.Page.onceFrameStoppedLoading(e => testRunner.log(`${prefix}: ${e.method}`));
  }

  setLogFrameLoadingEvents(dp, 'main target');
  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  const url1 = 'http://127.0.0.1:8000/inspector-protocol/resources/empty.html';
  const url2 = `${url1}?foo=bar`;
  const url3 = `http://oopif-a.devtools.test:8000/inspector-protocol/resources/empty.html`;
  const url4 = `http://oopif-b.devtools.test:8000/inspector-protocol/resources/empty.html`;

  testRunner.log(`Initial navigation`);
  session.evaluate(`document.getElementById('frame').src = '${url1}'`);
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(`Navigating same origin`);
  session.evaluate(`document.getElementById('frame').src = '${url2}'`);
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(`Navigating cross origin`);

  session.evaluate(`document.getElementById('frame').src = '${url3}'`);

  let {params} = await dp.Target.onceAttachedToTarget();
  const session2 = session.createChild(params.sessionId);
  const dp2 = session2.protocol;
  setLogFrameLoadingEvents(dp2, 'child target');
  dp2.Runtime.runIfWaitingForDebugger();

  await dp2.Page.onceDomContentEventFired();
  testRunner.log(await session2.evaluate(`location.href`));

  testRunner.log(`Navigating cross origin again`);
  session.evaluate(`document.getElementById('frame').src = '${url4}'`);

  await dp2.Page.onceDomContentEventFired();
  testRunner.log(await session2.evaluate(`location.href`));

  testRunner.log(`Navigating back to in-process`);
  session.evaluate(`document.getElementById('frame').src = '${url1}'`);
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(`Navigating main frame to same origin`);
  session.navigate(url1);
  await dp.Page.onceFrameStoppedLoading();

  testRunner.log(`Navigating main frame cross-process`);
  session.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');
  await dp.Page.onceFrameStoppedLoading();

  testRunner.completeTest();
})

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that RTCPeerConnection.createDTMFSender() reports a deprecation issue.`);
  await dp.Audits.enable();
  const promise = dp.Audits.onceIssueAdded();
  session.evaluate(`
    const pc = new RTCPeerConnection();
    const stream = new AudioContext().createMediaStreamDestination().stream;
    pc.addStream(stream);
    pc.createDTMFSender(stream.getAudioTracks()[0]);
  `);
  const result = await promise;
  testRunner.log(result.params, 'Inspector issue: ');
  testRunner.completeTest();
})

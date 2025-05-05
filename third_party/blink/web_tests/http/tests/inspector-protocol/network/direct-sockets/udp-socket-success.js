(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      `https://devtools.oopif.test:8443/inspector-protocol/network/direct-sockets/resources/udp-socket-success.php`,
      `UDP DirectSockets success`);

  await dp.Network.enable({reportDirectSocketTraffic: true});
  session.evaluate('openSocket()');

  const createdEvent = await dp.Network.onceDirectUDPSocketCreated();
  testRunner.log('socket created');
  testRunner.log(createdEvent.params.options, '   options: ')
  const initiator = createdEvent.params.initiator;
  testRunner.log('');
  testRunner.log('   Initiator Type: ' + initiator.type);
  const callFrames = initiator.stack ? initiator.stack.callFrames : [];
  for (let i = 0; i < callFrames.length; ++i) {
    const frame = callFrames[i];
    testRunner.log('   Stack #' + i);
    if (frame.lineNumber) {
      testRunner.log('     functionName: ' + frame.functionName);
      testRunner.log('     url: ' + cleanUrl(frame.url));
      testRunner.log('     lineNumber: ' + frame.lineNumber);
      break;
    }
  }

  const openedEvent = await dp.Network.onceDirectUDPSocketOpened();
  testRunner.log('socket opened');
  testRunner.log('   localAddr: ' + openedEvent.params.localAddr);
  testRunner.log('   remoteAddr: ' + openedEvent.params.remoteAddr);

  const chunkSentEvent = await dp.Network.onceDirectUDPSocketChunkSent();
  testRunner.log('socket chunk sent');
  testRunner.log(
      '   message.data: ' + base64ToUtf8(chunkSentEvent.params.message.data));
  testRunner.log(
      '   message.remoteAddr: ' + chunkSentEvent.params.message.remoteAddr);

  const chunkReceivedEvent =
      await dp.Network.onceDirectUDPSocketChunkReceived();
  testRunner.log('socket chunk received');
  testRunner.log(
      '   message.data: ' +
      base64ToUtf8(chunkReceivedEvent.params.message.data));
  testRunner.log(
      '   message.remoteAddr: ' + chunkReceivedEvent.params.message.remoteAddr);

  await dp.Network.onceDirectUDPSocketClosed();
  testRunner.log('socket closed');

  testRunner.completeTest();
})

function base64ToUtf8(base64String) {
  const binaryString = atob(base64String);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  const decoder = new TextDecoder('utf-8');
  return decoder.decode(bytes);
}

function cleanUrl(url) {
  match = url.match(/\/[^\/]+$/);
  if (match.length)
    return match[0].substr(1);
  return url;
}

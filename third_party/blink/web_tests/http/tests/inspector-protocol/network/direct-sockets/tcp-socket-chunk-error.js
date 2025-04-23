(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      `https://devtools.oopif.test:8443/inspector-protocol/network/direct-sockets/resources/tcp-socket-default.php`,
      `TCP DirectSockets chunk error`);

  await dp.Network.enable();
  session.evaluate(`
    (async () => {
      const serverSocket = new TCPServerSocket("127.0.0.1");
      const { localPort } = await serverSocket.opened;
      const clientSocket = new TCPSocket("127.0.0.1", localPort);

      const clientSocketSend = (async () => {
        const { writable } = await clientSocket.opened;
        const writer = writable.getWriter();
        await writer.ready;
        // This is a mistake to send string directly without encoding.
        await writer.write("I'm a netcat. Meow-meow!");
        writer.releaseLock();
      })();

      await clientSocketSend;
      await clientSocket.close();
      await serverSocket.close();
    })();
  `);

  const createdEvent = await dp.Network.onceDirectTCPSocketCreated();
  testRunner.log('socket created');
  testRunner.log('   remoteAddr: ' + createdEvent.params.remoteAddr);

  const openedEvent = await dp.Network.onceDirectTCPSocketOpened();
  testRunner.log('socket opened');
  testRunner.log('   remoteAddr: ' + openedEvent.params.remoteAddr);
  testRunner.log('   localAddr: ' + openedEvent.params.remoteAddr);

  const chunkErrorEvent = await dp.Network.onceDirectTCPSocketChunkError();
  testRunner.log('socket chunk error');
  testRunner.log('   errorMessage: ' + chunkErrorEvent.params.errorMessage);

  testRunner.completeTest();
})

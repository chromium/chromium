(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      `https://devtools.oopif.test:8443/inspector-protocol/network/direct-sockets/resources/socket-default.php`,
      `TCP DirectSockets abort`);

  await dp.Network.enable();

  session.evaluate(`
    new TCPSocket("127.0.0.1", 468,
  { noDelay: true, receiveBufferSize: 10011,
    sendBufferSize: 10022, keepAliveDelay: 10033, dnsQueryType: "ipv6" });
    `);

  const createdEvent = await dp.Network.onceDirectTCPSocketCreated();
  testRunner.log('socket created');
  testRunner.log('   remoteAddr: ' + createdEvent.params.remoteAddr);

  const abortedEvent = await dp.Network.onceDirectTCPSocketAborted();
  testRunner.log('socket aborted');
  testRunner.log('   errorMessage: ' + abortedEvent.params.errorMessage);

  testRunner.completeTest();
})

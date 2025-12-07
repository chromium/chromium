(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      `https://devtools.oopif.test:8443/inspector-protocol/network/direct-sockets/resources/socket-default.php`,
      `UDP DirectSockets abort`);

  await dp.Network.enable();

  session.evaluate(`
    new UDPSocket({ remoteAddress: "fakedomain.com", remotePort: 100, dnsQueryType: "ipv6", multicastLoopback: false, multicastTimeToLive: 10 });
    `);

  const createdEvent = await dp.Network.onceDirectUDPSocketCreated();
  testRunner.log('socket created');
  testRunner.log('   remoteAddr: ' + createdEvent.params.options.remoteAddr);
  testRunner.log('   remotePort: ' + createdEvent.params.options.remotePort);

  const abortedEvent = await dp.Network.onceDirectUDPSocketAborted();
  testRunner.log('socket aborted');
  testRunner.log('   errorMessage: ' + abortedEvent.params.errorMessage);

  testRunner.completeTest();
})

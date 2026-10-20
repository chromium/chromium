(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      `https://devtools.oopif.test:8443/inspector-protocol/network/direct-sockets/resources/socket-default.php`,
      `UDP DirectSockets multicast membership events`);

  await dp.Network.enable();
  const logEvent = name => ({params}) => testRunner.log(
      `${name}: IPAddress=${params.IPAddress} sourceAddress=${params.sourceAddress}`);
  dp.Network.onDirectUDPSocketJoinedMulticastGroup(logEvent('joined'));
  dp.Network.onDirectUDPSocketLeftMulticastGroup(logEvent('left'));

  const error = await session.evaluateAsync(`(async () => {
    try {
      const socket = new UDPSocket({localAddress: '0.0.0.0'});
      const {multicastController} = await socket.opened;
      await multicastController.joinGroup('237.132.100.17');
      await multicastController.leaveGroup('237.132.100.17');
      const ssm = {sourceAddress: '192.0.2.1'};
      await multicastController.joinGroup('232.1.1.1', ssm);
      await multicastController.leaveGroup('232.1.1.1', ssm);
      await socket.close();
    } catch (e) {
      return String(e);
    }
  })()`);
  if (error) {
    testRunner.log(error);
  }

  testRunner.completeTest();
})

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const assertEq = (actual, expected) => {
  if (actual !== expected) {
    throw `Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`;
  }
};

// It is fine to reuse the same address, because
// the port will be generated unique on this machine.
// multicast addresses are in range 224.0.0.0 to 239.255.255.255.
const multicastGroupAddress = '237.132.100.17';

async function launchUdpEchoServer(server, requiredBytes, clientAddress, clientPort) {
  let bytesEchoed = 0;

  const { readable, writable } = await server.opened;
  const reader = readable.getReader();
  const writer = writable.getWriter();

  while (bytesEchoed < requiredBytes) {
    const { value: { data, remoteAddress, remotePort }, done } = await reader.read();
    assertEq(done, false);
    assertEq(remoteAddress, clientAddress);
    assertEq(remotePort, clientPort);
    for (let index = 0; index < data.length; index++) {
      assertEq(data[index], bytesEchoed % 256);
      bytesEchoed++;
    }
    await writer.write({ data, remoteAddress, remotePort });
  }

  assertEq(bytesEchoed, requiredBytes);
  reader.releaseLock();
  writer.releaseLock();
}

async function sendLoop(socket, requiredBytes) {
  let bytesWritten = 0;
  let chunkLength = 0;

  const { writable } = await socket.opened;
  const writer = writable.getWriter();

  while (bytesWritten < requiredBytes) {
    chunkLength = Math.min(chunkLength + 1,
                           requiredBytes - bytesWritten);
    let chunk = new Uint8Array(chunkLength);
    for (let index = 0; index < chunkLength; index++) {
      chunk[index] = bytesWritten % 256;
      bytesWritten++;
    }
    await writer.ready;
    await writer.write({ data: chunk });
  }
  assertEq(bytesWritten, requiredBytes);

  writer.releaseLock();
}

async function readLoop(socket, requiredBytes) {
  let bytesRead = 0;

  const { readable } = await socket.opened;
  const reader = readable.getReader();

  while (bytesRead < requiredBytes) {
    const { value: { data }, done } = await reader.read();
    assertEq(done, false);
    for (let index = 0; index < data.length; index++) {
      assertEq(data[index], bytesRead % 256);
      bytesRead++;
    }
  }
  assertEq(bytesRead, requiredBytes);

  reader.releaseLock();
}

async function closeUdp(options) {
  try {
    let udpSocket = new UDPSocket(options);
    await udpSocket.opened;
    await udpSocket.close();
    return 'closeUdp succeeded';
  } catch (error) {
    return ('closeUdp failed: ' + error);
  }
}

async function sendUdpAfterClose(options, requiredBytes) {
  try {
    let udpSocket = new UDPSocket(options);
    let { writable } = await udpSocket.opened;
    await udpSocket.close();

    return await sendLoop(udpSocket, requiredBytes);
  } catch (error) {
    return ('send failed: ' + error);
  }
}

async function readUdpAfterSocketClose(options) {
  try {
    let udpSocket = new UDPSocket(options);
    let { readable, writable } = await udpSocket.opened;
    let reader = readable.getReader();
    let writer = writable.getWriter();
    let rp = reader.read().catch(() => {});
    await reader.cancel();
    await writer.abort();
    await rp;
    return 'readUdpAferSocketClose succeeded.';
  } catch (error) {
    return ('readUdpAfterSocketClose failed: ' + error);
  }
}

async function joinGroup(options, ipAddress) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(ipAddress);
    await udpSocket.close();
    return 'joinGroup succeeded.';
  } catch (error) {
    return ('joinGroup failed: ' + error);
  }
}

async function joinGroupTwice(options) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(multicastGroupAddress);
    await multicastController.joinGroup(multicastGroupAddress);
    await udpSocket.close();
    return 'joinGroupTwice succeeded.';
  } catch (error) {
    return ('joinGroupTwice failed: ' + error);
  }
}

async function leaveGroupAfterJoin(options) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;
    assertEq(multicastController.joinedGroups.length, 0);

    await multicastController.joinGroup(multicastGroupAddress);

    assertEq(multicastController.joinedGroups.length, 1);
    assertEq(multicastController.joinedGroups[0], multicastGroupAddress);

    await multicastController.leaveGroup(multicastGroupAddress);
    assertEq(multicastController.joinedGroups.length, 0);

    await udpSocket.close();
    return 'leaveGroupAfterJoin succeeded.';
  } catch (error) {
    return ('leaveGroupAfterJoin failed: ' + error);
  }
}

async function leaveGroupTwiceAfterJoin(options) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(multicastGroupAddress);
    await multicastController.leaveGroup(multicastGroupAddress);
    await multicastController.leaveGroup(multicastGroupAddress);

    await udpSocket.close();
    return 'leaveGroupTwiceAfterJoin succeeded.';
  } catch (error) {
    return ('leaveGroupTwiceAfterJoin failed: ' + error);
  }
}

async function joinGroupAfterClose(options) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;
    await udpSocket.close();

    await multicastController.joinGroup(multicastGroupAddress);
    return 'joinGroupAfterClose succeeded.';
  } catch (error) {
    return ('joinGroupAfterClose failed: ' + error);
  }
}

async function leaveGroupAfterClose(options) {
  try {
    let udpSocket = new UDPSocket(options);
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(multicastGroupAddress);
    await udpSocket.close();

    await multicastController.leaveGroup(multicastGroupAddress);

    return 'leaveGroupAfterClose succeeded.';
  } catch (error) {
    return ('leaveGroupAfterClose failed: ' + error);
  }
}

async function readUdpAfterStreamClose(options) {
  try {
    let udpSocket = new UDPSocket(options);
    let { readable } = await udpSocket.opened;
    let reader = readable.getReader();
    let rp = reader.read().catch(() => {});
    await reader.cancel();
    let { value, done } = await rp;
    if (!done) {
      return 'Stream is not closed!';
    }
    return 'readUdpAferStreamClose succeeded.';
  } catch (error) {
    return ('readUdpAfterStreamClose failed: ' + error);
  }
}

async function closeUdpWithLockedReadable(options, unlock = false) {
  try {
    let udpSocket = new UDPSocket(options);
    let { readable } = await udpSocket.opened;

    let reader = readable.getReader();

    if (unlock) {
      reader.releaseLock();
    }

    await udpSocket.close();

    if (unlock) {
      await udpSocket.closed;
    }

    return 'closeUdpWithLockedReadable succeeded.';
  } catch (error) {
    return 'closeUdpWithLockedReadable failed: ' + error;
  }
}

async function read(reader) {
  return reader.read().then(() => true, () => false);
}

async function write(writer, value) {
  const encoder = new TextEncoder();
  return writer.write({ data: encoder.encode(value) }).then(() => true, () => false);
}

async function readWriteUdpOnError(socket) {
  try {
    let { readable, writable } = await socket.opened;

    let reader = readable.getReader();
    let writer = writable.getWriter();

    let rp = read(reader);
    let wp = write(writer, '_');

    let [read_request_success, write_request_success] = await Promise.all([rp, wp]);

    if (!read_request_success && !write_request_success) {
      return 'readWriteUdpOnError succeeded.';
    } else {
      throw new TypeError(`read_request_success = ${read_request_success}, write_request_success = ${write_request_success}`);
    }
  } catch (error) {
    return 'readWriteUdpOnError failed: ' + error;
  }
}

async function multicastControllerAbsent(options) {
  try {
    let socket = new UDPSocket(options);
    let {multicastController} = await socket.opened;
    let hasMulticast = multicastController != null;

    await socket.close();

    if (hasMulticast) {
      throw new TypeError('multicast must be absent');
    } else {
      return 'multicastControllerAbsent succeeded.';
    }
  } catch (error) {
    return 'multicastControllerAbsent failed: ' + error;
  }
}

async function exchangeUdpPacketsBetweenClientAndServer() {
  const kRequiredDatagrams = 35;
  const kRequiredBytes =
    kRequiredDatagrams * (kRequiredDatagrams + 1) / 2;

  try {
    // |localPort| is intentionally omitted so that the OS will pick one itself.
    const serverSocket = new UDPSocket({ localAddress: "127.0.0.1" });
    const { localPort: serverSocketPort } = await serverSocket.opened;

    // Connect a client to the server.
    const clientSocket = new UDPSocket({
      remoteAddress: "127.0.0.1",
      remotePort: serverSocketPort
    });
    const { localAddress: clientLocalAddress, localPort: clientLocalPort } = await clientSocket.opened;

    launchUdpEchoServer(serverSocket, kRequiredBytes, clientLocalAddress, clientLocalPort);
    sendLoop(clientSocket, kRequiredBytes);
    await readLoop(clientSocket, kRequiredBytes);

    await clientSocket.close();
    await serverSocket.close();

    return "exchangeUdpPacketsBetweenClientAndServer succeeded.";
  } catch (error) {
    return "exchangeUdpPacketsBetweenClientAndServer failed: " + error;
  }
}

async function exchangeUdpMulticastPackets() {
  const kRequiredDatagrams = 35;
  const kRequiredBytes = kRequiredDatagrams * (kRequiredDatagrams + 1) / 2;

  try {
    const receiverSocket = new UDPSocket({
      localAddress: '0.0.0.0',
    });
    const {localPort: multicastPort, multicastController} =
        await receiverSocket.opened;
    multicastController.joinGroup(multicastGroupAddress);

    const senderSocket = new UDPSocket({
      remoteAddress: multicastGroupAddress,
      remotePort: multicastPort,
      multicastTimeToLive: 0,
      multicastLoopback: true
    });

    const sendLoopPromise = sendLoop(senderSocket, kRequiredBytes);
    const readLoopPromise = readLoop(receiverSocket, kRequiredBytes);

    await Promise.all([sendLoopPromise, readLoopPromise]);

    await senderSocket.close();
    await receiverSocket.close();

    return 'exchangeUdpMulticastPackets succeeded.';
  } catch (error) {
    return 'exchangeUdpMulticastPackets failed: ' + error;
  }
}

async function exchangeUdpMulticastPacketsMultipleReceivers() {
  const kRequiredDatagrams = 35;
  const kRequiredBytes = kRequiredDatagrams * (kRequiredDatagrams + 1) / 2;

  try {
    // Create receiver 1.
    const receiverSocket1 = new UDPSocket(
        {localAddress: '0.0.0.0', multicastAllowAddressSharing: true});
    const {
      localPort: multicastPort,
      multicastController: multicastController1
    } = await receiverSocket1.opened;
    multicastController1.joinGroup(multicastGroupAddress);

    // Create receiver 2.
    const receiverSocket2 = new UDPSocket({
      localAddress: '0.0.0.0',
      localPort: multicastPort,
      multicastAllowAddressSharing: true
    });
    const {multicastController: multicastController2} =
        await receiverSocket2.opened;
    multicastController2.joinGroup(multicastGroupAddress);

    const senderSocket = new UDPSocket({
      remoteAddress: multicastGroupAddress,
      remotePort: multicastPort,
      multicastTimeToLive: 0,
      multicastLoopback: true
    });

    const readLoopPromise1 = readLoop(receiverSocket1, kRequiredBytes);
    const readLoopPromise2 = readLoop(receiverSocket2, kRequiredBytes);
    const sendLoopPromise = sendLoop(senderSocket, kRequiredBytes);

    await Promise.all([sendLoopPromise, readLoopPromise1, readLoopPromise2]);

    await senderSocket.close();
    await receiverSocket1.close();
    await receiverSocket2.close();

    return 'exchangeUdpMulticastPackets succeeded.';
  } catch (error) {
    return 'exchangeUdpMulticastPackets failed: ' + error;
  }
}

async function testUdpMessageConfiguration(socketOptions, message) {
  try {
    const socket = new UDPSocket(socketOptions);
    const { writable } = await socket.opened;
    const writer = writable.getWriter();

    await writer.write(message);
    return "testUdpMessageConfiguration succeeded.";
  } catch (error) {
    return "testUdpMessageConfiguration failed: " + error;
  }
}

// SSM (Source-Specific Multicast) test functions

const ssmMulticastGroup = '232.1.1.1';  // SSM range for IPv4
// Note: Source addresses are discovered and passed by C++ browser tests

async function joinGroupSSM(options, groupAddress, sourceAddress) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(
        groupAddress, {sourceAddress: sourceAddress});
    return 'joinGroupSSM succeeded.';
  } catch (error) {
    return ('joinGroupSSM failed: ' + error);
  } finally {
    await udpSocket.close();
  }
}

async function joinGroupSSMSameGroupDifferentSources(
    options, source1, source2) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    // Join same group with different sources - should succeed.
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: source1});
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: source2});

    assertEq(multicastController.joinedGroups.length, 2);
    // joinedGroups iterates a hash set, so order is non-deterministic.
    const sources = multicastController.joinedGroups
        .map(m => m.sourceAddress).sort();
    const expectedSources = [source1, source2].sort();
    assertEq(sources[0], expectedSources[0]);
    assertEq(sources[1], expectedSources[1]);
    // Both entries should reference the same multicast group.
    assertEq(
        multicastController.joinedGroups[0].groupAddress, ssmMulticastGroup);
    assertEq(
        multicastController.joinedGroups[1].groupAddress, ssmMulticastGroup);

    return 'joinGroupSSMSameGroupDifferentSources succeeded.';
  } catch (error) {
    return 'joinGroupSSMSameGroupDifferentSources failed: ' + error;
  } finally {
    await udpSocket.close();
  }
}

async function leaveGroupSSMMustMatchSource(options, source1, source2) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    // Join with source1.
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: source1});

    // Try to leave with different source - should throw.
    await multicastController.leaveGroup(
        ssmMulticastGroup, {sourceAddress: source2});
    // If we reach here, test failed - should have thrown
    return 'UNEXPECTED: leaveGroup with mismatched source should have failed';
  } catch (e) {
    if (e.name !== 'InvalidStateError') {
      return 'leaveGroupSSMMustMatchSource failed: unexpected error: ' + e;
    }
    return 'leaveGroupSSMMustMatchSource succeeded.';
  } finally {
    await udpSocket.close();
  }
}

async function cannotMixASMAndSSM(options, sourceIP) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    // Join with ASM (no source).
    await multicastController.joinGroup(ssmMulticastGroup);

    // Try to join same group with SSM - should throw.
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: sourceIP});
    // If we reach here, test failed
    return 'UNEXPECTED: mixing ASM and SSM should have failed';
  } catch (e) {
    if (e.name !== 'InvalidStateError') {
      return 'cannotMixASMAndSSM failed: unexpected error: ' + e;
    }
    return 'cannotMixASMAndSSM succeeded.';
  } finally {
    await udpSocket.close();
  }
}

async function joinGroupSSMTwiceWithSameSource(options, sourceIP) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: sourceIP});

    // Try to join again with same group and source - should throw.
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: sourceIP});
    // If we reach here, test failed
    return 'UNEXPECTED: duplicate SSM join should have failed';
  } catch (e) {
    return 'joinGroupSSMTwiceWithSameSource succeeded.';
  } finally {
    await udpSocket.close();
  }
}

async function joinGroupSSMInvalidSource(options) {
  let udpSocket = new UDPSocket(options);
  try {
    const {multicastController} = await udpSocket.opened;

    // Try to join with invalid source address - should throw.
    await multicastController.joinGroup(
        ssmMulticastGroup, {sourceAddress: 'invalid-source-address'});
    // If we reach here, test failed
    return 'UNEXPECTED: invalid source address should have failed';
  } catch (e) {
    return 'joinGroupSSMInvalidSource succeeded.';
  } finally {
    await udpSocket.close();
  }
}

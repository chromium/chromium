'use strict';

const assertEq = (actual, expected) => {
  if (actual !== expected) {
    throw `Expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`;
  }
};

async function sendLoop(writer, requiredBytes) {
  let bytesWritten = 0;
  let chunkLength = 0;

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
  return 'send succeeded';
}

async function readLoop(reader, requiredBytes) {
  let bytesRead = 0;
  while (bytesRead < requiredBytes) {
    const { value, done } = await reader.read();
    if (done) {
      return 'readLoop failed: stream closed prematurely.';
    }

    const { data } = value;
    if (!data || data.length === 0) {
      return 'readLoop failed: no data returned.';
    }

    for (let index = 0; index < data.length; index++) {
      if (data[index] != bytesRead % 256) {
        console.log(`Expected ${bytesRead % 256}, received ${data[index]}`);
        return 'readLoop failed: bad data.';
      }
      bytesRead++;
    }
  }
  return 'readLoop succeeded.';
}

async function sendUdp(options, requiredBytes) {
  try {
    let udpSocket = new UDPSocket(options);
    let { writable } = await udpSocket.opened;
    return await sendLoop(writable.getWriter(), requiredBytes);
  } catch (error) {
    return ('sendUdp failed: ' + error);
  }
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

    const writer = writable.getWriter();
    return await sendLoop(writer, requiredBytes);
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

async function exchangeSingleUdpPacketBetweenClientAndServer() {
  const kPacket = "I'm a UDP packet. Meow-meow!";

  try {
    // |localPort| is intentionally omitted so that the OS will pick one itself.
    const serverSocket = new UDPSocket({ localAddress: "127.0.0.1" });
    const { localPort: serverSocketPort } = await serverSocket.opened;

    // Connect a client to the server.
    const clientSocket = new UDPSocket({
      remoteAddress: "127.0.0.1",
      remotePort: serverSocketPort
    });

    const encoder = new TextEncoder();
    const decoder = new TextDecoder();

    // Waits for a packet to arrive and then echoes it back to
    // the original sender.
    async function serverEcho() {
      const {
        readable: serverReadable,
        writable: serverWritable,
      } = await serverSocket.opened;

      const reader = serverReadable.getReader();
      const writer = serverWritable.getWriter();

      const { value: {
        data,
        remoteAddress: packetRemoteAddress,
        remotePort: packetRemotePort
      }, done } = await reader.read();

      // Stream shouldn't be exhausted yet.
      assertEq(done, false);

      // Data should match.
      assertEq(kPacket, decoder.decode(data));

      const {
        localAddress: clientLocalAddr,
        localPort: clientLocalPort
      } = await clientSocket.opened;

      // Check that the packet arrived from the client.
      assertEq(clientLocalAddr, packetRemoteAddress);
      assertEq(clientLocalPort, packetRemotePort);

      // Echo back.
      await writer.ready;
      writer.write({
        data,
        remoteAddress: clientLocalAddr,
        remotePort: clientLocalPort
      });

      reader.releaseLock();
      writer.releaseLock();
    }

    serverEcho();

    // Sends the initial packet from client to server.
    async function clientSend() {
      const { writable: clientWritable } = await clientSocket.opened;
      const writer = clientWritable.getWriter();
      writer.write({ data: encoder.encode(kPacket) });
      writer.releaseLock();
    }

    clientSend();

    // Waits for the server to respond and verifies that the packet received
    // is indeed the original one.
    async function clientReceive() {
      const { readable: clientReadable } = await clientSocket.opened;
      const reader = clientReadable.getReader();

      const { value: {
        data, remoteAddress, remotePort
      }, done } = await reader.read();
      reader.releaseLock();

      // Stream shouldn't be exhausted yet.
      assertEq(done, false);

      // Data should match.
      assertEq(kPacket, decoder.decode(data));

      // In connected mode remoteAddress/remotePort should be undefined.
      assertEq(remoteAddress, undefined);
      assertEq(remotePort, undefined);
    }

    await clientReceive();

    await clientSocket.close();
    await serverSocket.close();

    return "exchangeSingleUdpPacketBetweenClientAndServer succeeded.";
  } catch (error) {
    return "exchangeSingleUdpPacketBetweenClientAndServer failed: " + error;
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

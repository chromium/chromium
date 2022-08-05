'use strict';

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

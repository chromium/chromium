'use strict';

async function writeLoop(writer, requiredBytes) {
  if (!(writer instanceof WritableStreamDefaultWriter))
    return 'write failed: writer is not a WritableStreamDefaultWriter';
  let bytesWritten = 0;
  let chunkLength = 0;
  while (bytesWritten < requiredBytes) {
    chunkLength = Math.min(chunkLength + 1,
                           requiredBytes - bytesWritten);
    let chunk = new Uint8Array(chunkLength);
    for (let index = 0; index < chunkLength; ++index) {
      chunk[index] = bytesWritten % 256;
      ++bytesWritten;
    }
    await writer.ready;
    await writer.write(chunk);
  }
  return 'write succeeded';
}

async function writeTcp(address, port, options, requiredBytes) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { writable } = await tcpSocket.opened;
    let writer = writable.getWriter();
    return await writeLoop(writer, requiredBytes);
  } catch(error) {
    return ('writeTcp failed: ' + error);
  }
}

async function writeLargeTcpPacket(address, port, size) {
  try {
    let tcpSocket = new TCPSocket(address, port);
    let { writable } = await tcpSocket.opened;
    let writer = writable.getWriter();

    let chunk = new Uint8Array(size);
    for (let index = 0; index < size; ++index) {
      chunk[index] = index % 256;
    }
    await writer.write(chunk);

    return 'writeLargeTcpPacket succeeded';
  } catch(error) {
    return ('writeLargeTcpPacket failed: ' + error);
  }
}

async function readLoop(reader, requiredBytes) {
  if (!(reader instanceof ReadableStreamDefaultReader))
    return 'read failed: reader is not a ReadableStreamDefaultReader';
  let bytesRead = 0;
  while (bytesRead < requiredBytes) {
    const { value, done } = await reader.read();
    if (done)
      return 'read failed: unexpected stream close';
    if (!value || value.length === 0)
      return 'read failed: no data returned';

    for (let index = 0; index < value.length; ++index) {
      if (value[index] !== bytesRead % 256)
        return 'read failed: bad data returned';
      ++bytesRead;
    }
  }
  return 'read succeeded';
}

async function readTcp(address, port, options, requiredBytes) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { readable } = await tcpSocket.opened;
    let reader = readable.getReader();
    return await readLoop(reader, requiredBytes);
  } catch(error) {
    return ('readTcp failed: ' + error);
  }
}

async function readWriteTcp(address, port, options, requiredBytes) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { readable, writable } = await tcpSocket.opened;
    let reader = readable.getReader();
    let writer = writable.getWriter();
    let [readResult, writeResult] =
        await Promise.all([readLoop(reader, requiredBytes),
                           writeLoop(writer, requiredBytes)]);
    if (readResult !== 'read succeeded')
      return readResult;
    if (writeResult !== 'write succeeded')
      return writeResult;
    return 'readWrite succeeded';
  } catch(error) {
    return ('readWriteTcp failed: ' + error);
  }
}

async function closeTcp(address, port, options) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { readable, writable } = await tcpSocket.opened;

    let reader = readable.getReader();
    let writer = writable.getWriter();

    reader.releaseLock();
    writer.releaseLock();

    let closed = tcpSocket.closed.then(() => true, () => false);
    await tcpSocket.close();

    if (await closed) {
      return 'closeTcp succeeded';
    }
  } catch (error) {
    return ('closeTcp failed: ' + error);
  }
}

async function read(reader) {
  return reader.read().then(() => true, () => false);
}

async function write(writer, value) {
  const encoder = new TextEncoder();
  return writer.write(encoder.encode(value)).then(() => true, () => false);
}

async function readTcpOnError(socket, expected_read_success) {
  try {
    let { readable } = await socket.opened;

    let reader = readable.getReader();

    let read_request_success = await read(reader);
    if (read_request_success === expected_read_success) {
      return 'readTcpOnError succeeded.';
    } else {
      throw new TypeError(`read_request_success = ${read_request_success}`);
    }
  } catch (error) {
    return 'readTcpOnError failed: ' + error;
  }
}

async function writeTcpOnError(socket) {
  try {
    let { writable } = await socket.opened;

    let writer = writable.getWriter();

    let write_request_success = await write(writer, '_'.repeat(3));
    if (!write_request_success) {
      return 'writeTcpOnError succeeded.';
    } else {
      throw new TypeError(`write_request_success = ${write_request_success}`);
    }
  } catch (error) {
    return 'writeTcpOnError failed: ' + error;
  }
}

async function readWriteTcpOnError(socket) {
  try {
    let { readable, writable } = await socket.opened;

    let reader = readable.getReader();
    let writer = writable.getWriter();

    let [read_request_success, write_request_success] = await Promise.all([read(reader), write(writer, '_'.repeat(3))]);
    if (!read_request_success && !write_request_success) {
      return 'readWriteTcpOnError succeeded.';
    } else {
      throw new TypeError(`read_request_success = ${read_request_success}, write_request_success = ${write_request_success}`);
    }
  } catch (error) {
    return 'readWriteTcpOnError failed: ' + error;
  }
}

async function waitForClosedPromise(socket, expected_closed_result, cancel_reader = false, close_writer = false) {
  try {
    let { readable, writable } = await socket.opened;

    let reader = readable.getReader();
    let writer = writable.getWriter();

    if (cancel_reader) {
      reader.cancel();
    }

    if (close_writer) {
      writer.close();
    }

    const closed_result = await socket.closed.then(() => true, () => false);

    if (closed_result === expected_closed_result) {
      return 'waitForClosedPromise succeeded.';
    } else {
      throw new TypeError(`closed_result = ${closed_result}, expected_close_result = ${expected_closed_result}`);
    }
  } catch (error) {
    return 'waitForClosedPromise failed: ' + error;
  }
}

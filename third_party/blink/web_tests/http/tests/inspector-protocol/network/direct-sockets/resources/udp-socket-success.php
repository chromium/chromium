<?php
header("Cross-Origin-Opener-Policy: same-origin");
header("Cross-Origin-Embedder-Policy: require-corp");
header("Permissions-Policy: direct-sockets=(self), direct-sockets-private=(self), direct-sockets-multicast=(self)");
header("Origin-Agent-Cluster: ?0");
?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <title>Direct socket test page with isolated context</title>
    <meta charset="utf-8">
  </head>
  <body>
    <script>
      async function openSocket() {
        const encoder = new TextEncoder();
        const connectedDatagram = encoder.encode("Hola from connected");
        const boundDatagram = encoder.encode("Bienvenido a bound")

        const boundSocket = new UDPSocket({
          localAddress: "127.0.0.1",
          sendBufferSize: 1002,
          receiveBufferSize: 1005,
          multicastLoopback: true,
          multicastTimeToLive: 64,
          multicastAllowAddressSharing: true
        });
        const { localPort: boundLocalPort } = await boundSocket.opened;

        const connectedSocket = new UDPSocket({
          remoteAddress: "127.0.0.1",
          remotePort: boundLocalPort,
        });

        const {
          localAddress: clientAddress,
          localPort: clientPort
        } = await connectedSocket.opened;

        const boundEchoLoop = (async() => {
          const { readable, writable } = await boundSocket.opened;
          const reader = readable.getReader();
          const writer = writable.getWriter();

          const { value: { data, remoteAddress, remotePort }, done } = await reader.read();

          await writer.write({ data: boundDatagram, remoteAddress, remotePort });

          reader.releaseLock();
          writer.releaseLock();
        })();

        (async () => {

          const { writable } = await connectedSocket.opened;
          const writer = writable.getWriter();
          await writer.ready;
          await writer.write({ data: connectedDatagram });
          writer.releaseLock();
        })();

        await (async () => {
          let bytesRead = 0;
          const requiredBytes = boundDatagram.byteLength;

          const { readable } = await connectedSocket.opened;
          const reader = readable.getReader();

          while (bytesRead < requiredBytes) {
            const { value: { data }, done } = await reader.read();
            bytesRead += data.length;
          }

          reader.releaseLock();
        })();
        await boundSocket.close();
        await connectedSocket.close();
      }
    </script>
  </body>
</html>

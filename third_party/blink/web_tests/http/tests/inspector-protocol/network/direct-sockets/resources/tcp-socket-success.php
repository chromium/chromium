<?php
header("Cross-Origin-Opener-Policy: same-origin");
header("Cross-Origin-Embedder-Policy: require-corp");
header("Permissions-Policy: direct-sockets=(self)");
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
        const kPacket = "I'm a netcat. Meow-meow!"

        const serverSocket = new TCPServerSocket("127.0.0.1");
        const { localPort } = await serverSocket.opened;

        const clientSocket = new TCPSocket("127.0.0.1", localPort,
        { noDelay: true, receiveBufferSize: 10011,
          sendBufferSize: 10022, keepAliveDelay: 10033, dnsQueryType: "ipv4" });

        const acceptedSocket = await (async () => {
          const { readable } = await serverSocket.opened;
          const reader = readable.getReader();
          const { value: acceptedSocket, done } = await reader.read();
          reader.releaseLock();
          return acceptedSocket;
        })();

        const encoder = new TextEncoder();
        const decoder = new TextDecoder();

        const readPacket = async socket => {
          const { readable } = await socket.opened;
          const reader = readable.getReader();
          let result = "";
          while (result.length < kPacket.length) {
            const { value, done } = await reader.read();
            result += decoder.decode(value);
          }
          reader.releaseLock();
        };

        const sendPacket = async socket => {
          const { writable } = await socket.opened;
          const writer = writable.getWriter();
          await writer.ready;
          await writer.write(encoder.encode(kPacket));
          writer.releaseLock();
        };

        const acceptedSocketEcho = (async () => {
          await readPacket(acceptedSocket);
          await sendPacket(acceptedSocket);
        })();

        const clientSocketSend = (async () => {
          await sendPacket(clientSocket);
        })();

        const clientSocketReceive = (async () => {
          await readPacket(clientSocket);
        })();


        await clientSocketReceive;
        await clientSocket.close();
        await acceptedSocket.close();
        await serverSocket.close();
      }
    </script>
  </body>
</html>

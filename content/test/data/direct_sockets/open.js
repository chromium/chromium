'use strict';

async function openTcp(address, port, options = {}) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { remoteAddress, remotePort } = await tcpSocket.connection;
    return ('openTcp succeeded: ' +
            '{remoteAddress: "' + remoteAddress +
            '", remotePort: ' + remotePort + '}');
  } catch(error) {
    return ('openTcp failed: ' + error);
  }
}

async function openUdp(address, port, options = {}) {
  try {
    let udpSocket = new UDPSocket(address, port, options);
    let { remoteAddress, remotePort } = await udpSocket.connection;
    return ('openUdp succeeded: ' +
            '{remoteAddress: "' + remoteAddress +
            '", remotePort: ' + remotePort + '}');
  } catch(error) {
    return ('openUdp failed: ' + error);
  }
}

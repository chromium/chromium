'use strict';

async function openTcp(address, port, options = {}) {
  try {
    let tcpSocket = new TCPSocket(address, port, options);
    let { remoteAddress, remotePort } = await tcpSocket.opened;
    return ('openTcp succeeded: ' +
            '{remoteAddress: "' + remoteAddress +
            '", remotePort: ' + remotePort + '}');
  } catch(error) {
    return ('openTcp failed: ' + error);
  }
}

async function openUdp(options) {
  try {
    let udpSocket = new UDPSocket(options);
    let { remoteAddress, remotePort } = await udpSocket.opened;
    return ('openUdp succeeded: ' +
            '{remoteAddress: "' + remoteAddress +
            '", remotePort: ' + remotePort + '}');
  } catch(error) {
    return ('openUdp failed: ' + error);
  }
}

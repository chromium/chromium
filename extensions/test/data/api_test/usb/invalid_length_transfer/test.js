// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var genericTransfer = {
  "direction": "in",
  "endpoint": 1,
  "length": -1
};
var controlTransfer = {
  "index": 0,
  "direction": "in",
  "requestType": "standard",
  "recipient": "device",
  "request": 0,
  "value": 0,
  "length": -1
};
var isoTransfer = {
  "packetLength": 0,
  "transferInfo": genericTransfer,
  "packets": 0
};
var errorTransferLength = 'Transfer length must be a ' +
    'positive number less than 104,857,600.';
var errorNumPackets = 'Number of packets must be a ' +
    'positive number less than 4,194,304.';
var errorPacketLength = 'Packet length must be a ' +
    'positive number less than 65,536.';
var errorInsufficientTransferLength = 'Transfer length is insufficient.';
var largeSize = 100 * 1024 * 1024 + 1;
var maxPackets = 4 * 1024 * 1024;
var maxPacketLength = 64 * 1024;

function createInvalidTransferTest(usbFunction, transferInfo, transferLength) {
  return function() {
    genericTransfer["length"] = transferLength;
    controlTransfer["length"] = transferLength;
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      usbFunction(device, transferInfo, chrome.test.callbackFail(
          errorTransferLength, function() {}));
    });
  };
}

function createInvalidPacketLengthTest(
    transferLength, packets, packetLength, errorMessage) {
  return function() {
    genericTransfer["length"] = transferLength;
    isoTransfer["packets"] = packets;
    isoTransfer["packetLength"] = packetLength;
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      usb.isochronousTransfer(device, isoTransfer,
          chrome.test.callbackFail(
              errorMessage, function() {}));
    });
  };
}

var tests = [
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, -1),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, -1),
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, largeSize),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, largeSize),

  createInvalidPacketLengthTest(1024, -1, 0, errorNumPackets),
  createInvalidPacketLengthTest(maxPackets, maxPackets, 0, errorNumPackets),
  createInvalidPacketLengthTest(1024, 100, -1, errorPacketLength),
  createInvalidPacketLengthTest(1024, 100, maxPacketLength, errorPacketLength),
  createInvalidPacketLengthTest(1024, 1025, 0, errorInsufficientTransferLength)
];

chrome.test.runTests(tests);

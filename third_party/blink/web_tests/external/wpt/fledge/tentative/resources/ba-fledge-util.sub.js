'use strict;'

let BA = {};

(function(BA) {
const TestPrivateKey = new Uint8Array([
  0xff, 0x1f, 0x47, 0xb1, 0x68, 0xb6, 0xb9, 0xea, 0x65, 0xf7, 0x97,
  0x4f, 0xf2, 0x2e, 0xf2, 0x36, 0x94, 0xe2, 0xf6, 0xb6, 0x8d, 0x66,
  0xf3, 0xa7, 0x64, 0x14, 0x28, 0xd4, 0x45, 0x35, 0x01, 0x8f
]);

const _hpkeModulePromise = import('../third_party/hpke-js/hpke.js');

function _get16(buffer, offset) {
  return buffer[offset] << 8 | buffer[offset + 1];
}

function _get32(buffer, offset) {
  return buffer[offset] << 24 | buffer[offset + 1] << 16 |
      buffer[offset + 2] << 8 | buffer[offset + 3];
}

function _put16(buffer, offset, val) {
  buffer[offset] = val >> 8;
  buffer[offset + 1] = val & 0xFF;
}

function _toArrayBuffer(typedArray) {
  return typedArray.buffer.slice(
      typedArray.byteOffset, typedArray.byteOffset + typedArray.byteLength);
}

function _bufferAsStream(buffer) {
  return new ReadableStream({
    start: controller => {
      controller.enqueue(buffer);
      controller.close();
    }
  });
}

// Returns an ArrayBuffer.
async function _applyTransform(inData, transform) {
  const resultResponse =
      new Response(_bufferAsStream(inData).pipeThrough(transform));
  const resultBlob = await resultResponse.blob();
  return await resultBlob.arrayBuffer();
}

// Returns an ArrayBuffer (promise).
async function _gunzip(inData) {
  const decompress = new DecompressionStream('gzip');
  return _applyTransform(inData, decompress);
}

function _decodeIgDataHeader(igData) {
  if (igData.length < 8) {
    throw 'Not enough data for B&A and OHTTP headers';
  }
  return {
    version: igData[0],
    keyId: igData[1],
    kemId: _get16(igData, 2),
    kdfId: _get16(igData, 4),
    aeadId: _get16(igData, 6),
    payload: igData.slice(8)
  };
}

// Splits up the actual B&A IG Data into the enc and ct portions
// for HPKE, using `suite` for sizing; and also figures out the appropriate
// info string.
function _splitIgDataPayloadIntoEncAndCt(header, suite) {
  const RequestMessageType = 'message/auction request';

  // From RFC 9458 (Oblivious HTTP):
  // "2.  Build a sequence of bytes (info) by concatenating the ASCII-
  //      encoded string "message/bhttp request"; a zero byte; key_id as an
  //      8-bit integer; plus kem_id, kdf_id, and aead_id as three 16-bit
  //      integers."
  // (except we use a different message type string).
  const infoLength = RequestMessageType.length + 1 + 1 + 6;
  let info = new Uint8Array(infoLength);
  for (let pos = 0; pos < RequestMessageType.length; ++pos) {
    info[pos] = RequestMessageType.charCodeAt(pos);
  }
  info[RequestMessageType.length] = 0;
  info[RequestMessageType.length + 1] = header.keyId;
  _put16(info, RequestMessageType.length + 2, header.kemId);
  _put16(info, RequestMessageType.length + 4, header.kdfId);
  _put16(info, RequestMessageType.length + 6, header.aeadId);
  return {
    info: info,
    enc: header.payload.slice(0, suite.kem.encSize),
    ct: header.payload.slice(suite.kem.encSize)
  };
}

// Unwraps the padding envelope.
function _decodeIgDataPaddingHeader(decryptedText) {
  let length = _get32(decryptedText, 1);
  let format = decryptedText[0];

  // We currently only support format 2, which version = 0, and gzip
  // compression.
  assert_equals(format, 2);
  return {
    format: format,
    data: decryptedText.slice(5, 5 + length)
  };
}

// Decodes the request payload produced by getInterestGroupAdAuctionData into
// {paddedSize: ..., message: ...}
BA.decodeInterestGroupData = async function(igData) {
  const hpke = await _hpkeModulePromise;

  // Decode B&A level headers, and check them.
  const header = _decodeIgDataHeader(igData);

  // Only version 0 in use now.
  assert_equals(header.version, 0);

  // Test config uses keyId = 0x12 only
  assert_equals(header.keyId, 0x12);

  // Current cipher config.
  assert_equals(header.kemId, hpke.KemId.DhkemX25519HkdfSha256);
  assert_equals(header.kdfId, hpke.KdfId.HkdfSha256);
  assert_equals(header.aeadId, hpke.AeadId.Aes256Gcm);

  const suite = new hpke.CipherSuite({
    kem: header.kemId,
    kdf: header.kdfId,
    aead: header.aeadId,
  });

  // Split up the ciphertext from encapsulated key, and also compute
  // the expected message info.
  const pieces = _splitIgDataPayloadIntoEncAndCt(header, suite);

  // We can now decode the ciphertext.
  const privateKey = await suite.kem.importKey('raw', TestPrivateKey);
  const recipient = await suite.createRecipientContext(
      {recipientKey: privateKey, info: pieces.info, enc: pieces.enc});
  const pt = new Uint8Array(await recipient.open(pieces.ct));

  // The resulting text has yet another envelope with version and size info,
  // and a bunch of padding.
  const withoutPadding = _decodeIgDataPaddingHeader(pt);
  const decoded = CBOR.decode(_toArrayBuffer(withoutPadding.data));

  // Decompress IGs, CBOR-decode them, and replace in-place.
  for (let key of Object.getOwnPropertyNames(decoded.interestGroups)) {
    let val = decoded.interestGroups[key];
    let decompressedVal = await _gunzip(val);
    decoded.interestGroups[key] = CBOR.decode(decompressedVal);
  }

  return {
    paddedSize: pt.length,
    message: decoded
  };
}
})(BA);

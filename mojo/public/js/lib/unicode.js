// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Defines functions for translating between JavaScript strings and UTF8 strings
 * stored in ArrayBuffers. There is much room for optimization in this code if
 * it proves necessary.
 */
(function() {
  var internal = mojo.internal;
  var textDecoder = new TextDecoder('utf-8');
  var textEncoder = new TextEncoder('utf-8');

  /**
   * Decodes the UTF8 string from the given buffer.
   * @param {ArrayBufferView} buffer The buffer containing UTF8 string data.
   * @return {string} The corresponding JavaScript string.
   */
  function decodeUtf8String(buffer) {
    return textDecoder.decode(buffer);
  }

  /**
   * Encodes the given JavaScript string into UTF8.
   * @param {string} str The string to encode.
   * @param {ArrayBufferView} outputBuffer The buffer to contain the result.
   * Should be pre-allocated to hold enough space. Use |utf8Length| to determine
   * how much space is required.
   * @return {number} The number of bytes written to |outputBuffer|.
   */
  function encodeUtf8String(str, outputBuffer) {
    const utf8Buffer = textEncoder.encode(str);
    if (outputBuffer.length < utf8Buffer.length)
      throw new Error("Buffer too small for encodeUtf8String");
    outputBuffer.set(utf8Buffer);
    return utf8Buffer.length;
  }

  /**
   * Returns the number of bytes that a UTF8 encoding of the JavaScript string
   * |str| would occupy.
   */
  function utf8Length(str) {
    return textEncoder.encode(str).length;
  }

  internal.decodeUtf8String = decodeUtf8String;
  internal.encodeUtf8String = encodeUtf8String;
  internal.utf8Length = utf8Length;
})();

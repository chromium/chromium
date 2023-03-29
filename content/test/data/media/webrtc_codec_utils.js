// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Webrtc codec test utilities.

// Promotes H264 to be the first in the m=video line, if available.
function maybePreferH264SendCodec(sdp) {
  var sdpLines = sdp.split('\r\n');

  var mLineIndex = findLine(sdpLines, 'm=', 'video');
  if (mLineIndex === null) {
    throw new Error('No |m=video| line found in the sdp.');
  }

  var payload = getCodecPayloadType(sdpLines, 'h264');
  if (payload === null) {
    console.log("H264 is not available on this target.");
    return sdp;
  }

  // If h264 is available, set it as the default in m line.
  sdpLines[mLineIndex] = setDefaultCodec(sdpLines[mLineIndex], payload);
  sdp = sdpLines.join('\r\n');
  return sdp;
}

// Find the line in sdpLines that starts with |prefix|, and, if specified,
// contains |substr| (case-insensitive search).
function findLine(sdpLines, prefix, substr) {
  for (var i = 0; i < sdpLines.length; ++i) {
    if (sdpLines[i].indexOf(prefix) === 0) {
      if (!substr ||
          sdpLines[i].toLowerCase().indexOf(substr.toLowerCase()) !== -1) {
        return i;
      }
    }
  }
  return null;
}

// Gets the codec payload type from sdp lines.
function getCodecPayloadType(sdpLines, codec) {
  var index = findLine(sdpLines, 'a=rtpmap', codec);
  if (index === null)
    return null;

  var pattern = new RegExp('a=rtpmap:(\\d+) [a-zA-Z0-9-]+\\/\\d+');
  var result = sdpLines[index].match(pattern);

  return (result && result.length === 2) ? result[1] : null;
}

// Returns a new m= line with the specified codec as the first one.
function setDefaultCodec(mLine, payload) {
  var elements = mLine.split(' ');
  // Just copy the first three parameters; codec order starts on fourth.
  var newLine = elements.slice(0, 3);
  // Put target payload first and copy in the rest.
  newLine.push(payload);
  for (var i = 3; i < elements.length; i++) {
    if (elements[i] !== payload) {
      newLine.push(elements[i]);
    }
  }
  return newLine.join(' ');
}

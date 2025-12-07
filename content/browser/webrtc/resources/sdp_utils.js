// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reduced copy of
// third_party/blink/web_tests/external/wpt/webrtc/third_party/sdp/sdp.js

// Splits SDP into lines, dealing with both CRLF and LF.
export function splitLines(blob) {
  return blob.trim().split('\n').map(line => line.trim());
}

// Splits SDP into sections, including the sessionpart.
export function splitSections(blob) {
  const parts = blob.split('\nm=');
  return parts.map((part, index) => (index > 0 ?
    'm=' + part : part).trim() + '\r\n');
}

// Gets the direction from the mediaSection or the sessionpart.
export function getDirection(mediaSection, sessionpart) {
  // Look for sendrecv, sendonly, recvonly, inactive, default to sendrecv.
  const lines = splitLines(mediaSection);
  for (let i = 0; i < lines.length; i++) {
    switch (lines[i]) {
      case 'a=sendrecv':
      case 'a=sendonly':
      case 'a=recvonly':
      case 'a=inactive':
        return lines[i].substring(2);
    }
  }
  if (sessionpart) {
    return getDirection(sessionpart);
  }
  return 'sendrecv';
}

// Parses a m= line.
export function parseMLine(mediaSection) {
  const lines = splitLines(mediaSection);
  const parts = lines[0].substring(2).split(' ');
  return {
    kind: parts[0],
    port: parseInt(parts[1], 10),
    protocol: parts[2],
    fmt: parts.slice(3).join(' '),
  };
}

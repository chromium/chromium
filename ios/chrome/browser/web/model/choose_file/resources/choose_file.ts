// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Obsers taps on Choose file inputs.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js'

const CHOOSE_FILE_INPUT_HANDLER_NAME = 'ChooseFileHandler';

// These lists are not exhaustive, but should give a good clue of what type
// are filtered.
const AUDIO_FILES = ['aac', 'flac', 'm4a', 'mp3', 'ogg', 'pcm', 'wav', 'wma'];
const IMAGE_FILES = [
  'apng', 'avif', 'gif', 'jpg', 'jpeg', 'jfif', 'pjpeg', 'pjp', 'png', 'svg',
  'webp'
];
const VIDEO_FILES = ['avi', 'flv', 'mkv', 'mov', 'mp4', 'mpeg', 'webm', 'wmv'];
const COMPRESSED_FILES = ['7z', 'cab', 'gz', 'rar', 'tar', 'zip'];
const PDF_FILES = ['pdf'];
const DOC_FILES = [
  'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'key', 'numbers', 'pages',
  'epub', 'txt'
];
const APPLE_FILES = ['pkpass', 'mobileprovision'];

// The accept type sent to the browser.
enum AcceptType {
  NoAccept = 0,
  MixedAccept = 1,
  UnknownAccept = 2,
  ImageAccept = 3,
  VideoAccept = 4,
  AudioAccept = 5,
  ArchiveAccept = 6,
  PDFAccept = 7,
  DocAccept = 8,
  AppleAccept = 9,
}

// Converts a single accept string to an AcceptType
function stringToAcceptType(acceptString: String): AcceptType {
  var accept = acceptString.trim().toLowerCase();
  if (accept === '') {
    return AcceptType.NoAccept;
  }
  if (accept.startsWith('image/')) {
    return AcceptType.ImageAccept;
  }
  if (accept.startsWith('video/')) {
    return AcceptType.VideoAccept;
  }
  if (accept.startsWith('audio/')) {
    return AcceptType.AudioAccept;
  }
  if (accept.startsWith('.')) {
    accept = accept.substring(1);
  }
  if (IMAGE_FILES.includes(accept)) {
    return AcceptType.ImageAccept;
  }
  if (VIDEO_FILES.includes(accept)) {
    return AcceptType.VideoAccept;
  }
  if (AUDIO_FILES.includes(accept)) {
    return AcceptType.AudioAccept;
  }
  if (COMPRESSED_FILES.includes(accept)) {
    return AcceptType.ArchiveAccept;
  }
  if (PDF_FILES.includes(accept)) {
    return AcceptType.PDFAccept;
  }
  if (DOC_FILES.includes(accept)) {
    return AcceptType.DocAccept;
  }
  if (APPLE_FILES.includes(accept)) {
    return AcceptType.AppleAccept;
  }
  return AcceptType.UnknownAccept;
}

// Converts a multiple accept string to an AcceptType
function MultipleStringToAcceptType(acceptString: String): AcceptType {
  const accepts = acceptString.split(',');
  var acceptType = AcceptType.NoAccept;
  for (const accept of accepts) {
    const current = stringToAcceptType(accept);
    if (acceptType == AcceptType.NoAccept ||
        acceptType == AcceptType.UnknownAccept) {
      acceptType = current;
      continue;
    }
    if (acceptType == current || current == AcceptType.UnknownAccept ||
        current == AcceptType.NoAccept) {
      continue;
    }
    // Types are different and neither is Unknown, so it is Mixed.
    return AcceptType.MixedAccept;
  }
  return acceptType;
}

// Returns whether `ch` is a string with a single UTF-16 code unit which is a
// valid RFC2616 token character. This is mimicking verifications done by Blink.
function isRFC2616TokenCharacter(ch: String): boolean {
  const code = ch.charCodeAt(0);
  if (code < 32 || code > 127) {
    return false;
  }
  switch (ch) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case String.fromCharCode(0x20):  // SP
    case String.fromCharCode(0x09):  // HT
    case String.fromCharCode(0x7f):  // DEL
      return false;
    default:
      return true;
  }
}

// Returns whether `mimeType` is a string which represents a valid MIME type.
function isValidMIMEType(mimeType: String): boolean {
  let slashPosition = mimeType.indexOf('/');
  if (slashPosition == -1 || slashPosition == mimeType.length - 1 ||
      slashPosition == 0) {
    return false;
  }
  for (let i = 0; i < mimeType.length; i++) {
    if (!isRFC2616TokenCharacter(mimeType[i]!) && i != slashPosition) {
      return false;
    }
  }
  return true;
}

// Returns whether `fileExtension` is a string which represents a valid file
// extension.
function isValidFileExtension(fileExtension: String): boolean {
  if (fileExtension.length < 2) {
    return false;
  }
  for (let i = 0; i < fileExtension.length; i++) {
    if (!isRFC2616TokenCharacter(fileExtension[i]!)) {
      return false;
    }
  }
  return fileExtension[0] == '.';
}

// Filters `acceptString` so it only contains valid MIME types.
function parseAcceptAttributeMimeTypes(acceptString: String): String {
  const acceptTypes = acceptString.split(',');
  var mimeTypes: String[] = [];
  for (const acceptType of acceptTypes) {
    const trimmedType = acceptType.trim();
    if (isValidMIMEType(trimmedType)) {
      mimeTypes.push(trimmedType);
    }
  }
  return mimeTypes.join(',');
}

// Filters `acceptString` so it only contains valid file extensions.
function parseAcceptAttributeFileExtensions(acceptString: String): String {
  const acceptTypes = acceptString.split(',');
  var fileExtensions: String[] = [];
  for (const acceptType of acceptTypes) {
    const trimmedType = acceptType.trim();
    if (isValidFileExtension(trimmedType)) {
      fileExtensions.push(trimmedType);
    }
  }
  return fileExtensions.join(',');
}

// Sends data about the event.
function processChooseFileClick(inputEvent: MouseEvent): void {
  if (!inputEvent.target || !(inputEvent.target instanceof HTMLInputElement)) {
    return;
  }
  const target = inputEvent.target as HTMLInputElement;
  if (target.type.toLowerCase() !== "file") {
    return;
  }
  var accept = AcceptType.NoAccept;
  var acceptString = target.getAttribute('accept')
  if (acceptString) {
    accept = MultipleStringToAcceptType(acceptString!);
  }

  acceptString = acceptString ? acceptString : '';
  const response = {
    'hasMultiple': target.hasAttribute('multiple'),
    'acceptType': accept,
    'mimeTypes': parseAcceptAttributeMimeTypes(acceptString),
    'fileExtensions': parseAcceptAttributeFileExtensions(acceptString),
  };

  sendWebKitMessage(CHOOSE_FILE_INPUT_HANDLER_NAME, response);
}

// Registers passive event listeners for the click on choose file inputs.
function registerChooseFileClick(): void {
  window.addEventListener(
      'click', processChooseFileClick, {capture: true, passive: true});
}

registerChooseFileClick();

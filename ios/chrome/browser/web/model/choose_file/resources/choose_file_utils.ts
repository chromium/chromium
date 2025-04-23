// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides common utilities to process taps on Choose file
 * inputs.
 */

// These lists are not exhaustive, but should give a good clue of what type
// are filtered.
const AUDIO_FILES = ['aac', 'flac', 'm4a', 'mp3', 'ogg', 'pcm', 'wav', 'wma'];
const IMAGE_FILES = [
  'apng',
  'avif',
  'gif',
  'jpg',
  'jpeg',
  'jfif',
  'pjpeg',
  'pjp',
  'png',
  'svg',
  'webp',
];
const VIDEO_FILES = ['avi', 'flv', 'mkv', 'mov', 'mp4', 'mpeg', 'webm', 'wmv'];
const COMPRESSED_FILES = ['7z', 'cab', 'gz', 'rar', 'tar', 'zip'];
const PDF_FILES = ['pdf'];
const DOC_FILES = [
  'doc',
  'docx',
  'xls',
  'xlsx',
  'ppt',
  'pptx',
  'key',
  'numbers',
  'pages',
  'epub',
  'txt',
];
const APPLE_FILES = ['pkpass', 'mobileprovision'];

// The accept type sent to the browser.
enum AcceptType {
  NO_ACCEPT = 0,
  MIXED_ACCEPT = 1,
  UNKNOWN_ACCEPT = 2,
  IMAGE_ACCEPT = 3,
  VIDEO_ACCEPT = 4,
  AUDIO_ACCEPT = 5,
  ARCHIVE_ACCEPT = 6,
  PDF_ACCEPT = 7,
  DOC_ACCEPT = 8,
  APPLE_ACCEPT = 9,
}

// Converts a single accept string to an AcceptType
function stringToAcceptType(acceptString: string): AcceptType {
  let accept = acceptString.trim().toLowerCase();
  if (accept === '') {
    return AcceptType.NO_ACCEPT;
  }
  if (accept.startsWith('image/')) {
    return AcceptType.IMAGE_ACCEPT;
  }
  if (accept.startsWith('video/')) {
    return AcceptType.VIDEO_ACCEPT;
  }
  if (accept.startsWith('audio/')) {
    return AcceptType.AUDIO_ACCEPT;
  }
  if (accept.startsWith('.')) {
    accept = accept.substring(1);
  }
  if (IMAGE_FILES.includes(accept)) {
    return AcceptType.IMAGE_ACCEPT;
  }
  if (VIDEO_FILES.includes(accept)) {
    return AcceptType.VIDEO_ACCEPT;
  }
  if (AUDIO_FILES.includes(accept)) {
    return AcceptType.AUDIO_ACCEPT;
  }
  if (COMPRESSED_FILES.includes(accept)) {
    return AcceptType.ARCHIVE_ACCEPT;
  }
  if (PDF_FILES.includes(accept)) {
    return AcceptType.PDF_ACCEPT;
  }
  if (DOC_FILES.includes(accept)) {
    return AcceptType.DOC_ACCEPT;
  }
  if (APPLE_FILES.includes(accept)) {
    return AcceptType.APPLE_ACCEPT;
  }
  return AcceptType.UNKNOWN_ACCEPT;
}

// Converts a multiple accept string to an AcceptType
function multipleStringToAcceptType(acceptString: string): AcceptType {
  const accepts = acceptString.split(',');
  let acceptType = AcceptType.NO_ACCEPT;
  for (const accept of accepts) {
    const current = stringToAcceptType(accept);
    if (acceptType === AcceptType.NO_ACCEPT ||
        acceptType === AcceptType.UNKNOWN_ACCEPT) {
      acceptType = current;
      continue;
    }
    if (acceptType === current || current === AcceptType.UNKNOWN_ACCEPT ||
        current === AcceptType.NO_ACCEPT) {
      continue;
    }
    // Types are different and neither is Unknown, so it is Mixed.
    return AcceptType.MIXED_ACCEPT;
  }
  return acceptType;
}

// Returns whether `ch` is a string with a single UTF-16 code unit which is a
// valid RFC2616 token character. This is mimicking verifications done by Blink.
function isRFC2616TokenCharacter(ch: string): boolean {
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
function isValidMIMEType(mimeType: string): boolean {
  const slashPosition = mimeType.indexOf('/');
  if (slashPosition === -1 || slashPosition === mimeType.length - 1 ||
      slashPosition === 0) {
    return false;
  }
  for (let i = 0; i < mimeType.length; i++) {
    if (!isRFC2616TokenCharacter(mimeType[i]!) && i !== slashPosition) {
      return false;
    }
  }
  return true;
}

// Returns whether `fileExtension` is a string which represents a valid file
// extension.
function isValidFileExtension(fileExtension: string): boolean {
  if (fileExtension.length < 2) {
    return false;
  }
  for (let i = 0; i < fileExtension.length; i++) {
    if (!isRFC2616TokenCharacter(fileExtension[i]!)) {
      return false;
    }
  }
  return fileExtension[0] === '.';
}

// Filters `acceptString` so it only contains valid MIME types.
function parseAcceptAttributeMimeTypes(acceptString: string): string {
  const acceptTypes = acceptString.split(',');
  const mimeTypes: string[] = [];
  for (const acceptType of acceptTypes) {
    const trimmedType = acceptType.trim();
    if (isValidMIMEType(trimmedType)) {
      mimeTypes.push(trimmedType);
    }
  }
  return mimeTypes.join(',');
}

// Filters `acceptString` so it only contains valid file extensions.
function parseAcceptAttributeFileExtensions(acceptString: string): string {
  const acceptTypes = acceptString.split(',');
  const fileExtensions: string[] = [];
  for (const acceptType of acceptTypes) {
    const trimmedType = acceptType.trim();
    if (isValidFileExtension(trimmedType)) {
      fileExtensions.push(trimmedType);
    }
  }
  return fileExtensions.join(',');
}

// Describes the state of a file input which was just clicked.
interface HtmlInputElementState {
  hasMultiple: boolean;
  acceptType: AcceptType;
  mimeTypes: string;
  fileExtensions: string;
  hasSelectedFile: boolean;
  documentContainsInput: boolean;
}

/**
 * If `target` has type `file`, returns the data describing the current state of
 * `target` after it was clicked. Otherwise returns `null`.
 *
 * @param target - The HTMLInputElement that was clicked.
 * @returns An object describing the state of the input element, or null if the
 *     target is not a file input.
 */
export function processHTMLInputElementClick(target: HTMLInputElement):
    HtmlInputElementState|null {
  if (target.type.toLowerCase() !== 'file') {
    return null;
  }
  let accept = AcceptType.NO_ACCEPT;
  let acceptString = target.getAttribute('accept');
  if (acceptString) {
    accept = multipleStringToAcceptType(acceptString!);
  }
  let hasFiles = false;
  if (target.files && target.files.length > 0) {
    hasFiles = true;
  }

  acceptString = acceptString ? acceptString : '';
  return {
    hasMultiple: target.hasAttribute('multiple'),
    acceptType: accept,
    mimeTypes: parseAcceptAttributeMimeTypes(acceptString),
    fileExtensions: parseAcceptAttributeFileExtensions(acceptString),
    hasSelectedFile: hasFiles,
    documentContainsInput: document.contains(target),
  };
}

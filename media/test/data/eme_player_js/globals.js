// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains global constant variables used by the application.

// Renewal message header. External Clear Key implementation returns this as
// part of renewal messages.
var EME_RENEWAL_MESSAGE_HEADER = 'RENEWAL';

// Default key used to encrypt many media files used in browser tests.
var KEY = new Uint8Array([0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
                          0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c]);

var DEFAULT_LICENSE_SERVER = document.location.origin + '/license_server';

var DEFAULT_MEDIA_FILE = 'http://shadi.kir/alcatraz/Chrome_44-enc_av.webm';

// Key ID used for init data.
var KEY_ID = '0123456789012345';

// Unique strings to identify test result expectations.
var UNIT_TEST_SUCCESS = 'UNIT_TEST_SUCCESS';
var UNIT_TEST_FAILURE = 'UNIT_TEST_FAILURE';
var NOTSUPPORTEDERROR = 'NOTSUPPORTEDERROR';
var EME_GENERATEREQUEST_FAILED = 'EME_GENERATEREQUEST_FAILED';
var EME_SESSION_NOT_FOUND = 'EME_SESSION_NOT_FOUND';
var EME_LOAD_FAILED = 'EME_LOAD_FAILED';
var EME_UPDATE_FAILED = 'EME_UPDATE_FAILED';
var EME_ERROR_EVENT = 'EME_ERROR_EVENT';
var EME_MESSAGE_UNEXPECTED_TYPE = 'EME_MESSAGE_UNEXPECTED_TYPE';
var EME_RENEWAL_MISSING_HEADER = 'EME_RENEWAL_MISSING_HEADER';
var EME_SESSION_CLOSED_AND_ERROR = 'EME_SESSION_CLOSED_AND_ERROR';

// Headers used when running some specific unittests in the external CDM.
var UNIT_TEST_RESULT_HEADER = 'UNIT_TEST_RESULT';

// Available EME key systems to use.
// TODO(xhwang): Unify naming in this list.
var WIDEVINE_KEYSYSTEM = 'com.widevine.alpha';
var CLEARKEY = 'org.w3.clearkey';
var EXTERNAL_CLEARKEY = 'org.chromium.externalclearkey';
var MEDIAFOUNDATION_CLEARKEY = 'org.chromium.externalclearkey.mediafoundation';
var MESSAGE_TYPE_TEST_KEYSYSTEM =
    'org.chromium.externalclearkey.messagetypetest';
var FILE_IO_TEST_KEYSYSTEM = 'org.chromium.externalclearkey.fileiotest';
var OUTPUT_PROTECTION_TEST_KEYSYSTEM =
    'org.chromium.externalclearkey.outputprotectiontest';
var PLATFORM_VERIFICATION_TEST_KEYSYSTEM =
    'org.chromium.externalclearkey.platformverificationtest';
var CRASH_TEST_KEYSYSTEM = 'org.chromium.externalclearkey.crash';
var VERIFY_HOST_FILES_TEST_KEYSYSTEM =
    'org.chromium.externalclearkey.verifycdmhosttest';
var STORAGE_ID_TEST_KEYSYSTEM = 'org.chromium.externalclearkey.storageidtest';

// Key system name:value map to show on the document page.
var KEY_SYSTEMS = {
  'Widevine': WIDEVINE_KEYSYSTEM,
  'Clearkey': CLEARKEY,
  'External Clearkey': EXTERNAL_CLEARKEY,
  'MediaFoundation Clearkey': MEDIAFOUNDATION_CLEARKEY
};

var CONFIG_CHANGE_TYPE = {
  CLEAR_TO_CLEAR : '0',
  CLEAR_TO_ENCRYPTED : '1',
  ENCRYPTED_TO_CLEAR : '2',
  ENCRYPTED_TO_ENCRYPTED : '3'
};

// General WebM and MP4 name:content_type map to show on the document page.
var MEDIA_TYPES = {
  'WebM - Audio Video': 'video/webm; codecs="vorbis, vp8"',
  'WebM - Video Only': 'video/webm; codecs="vp8"',
  'WebM - Audio Only': 'video/webm; codecs="vorbis"',
  'MP4 - Video Only': 'video/mp4; codecs="avc1.4D000C"',
  'MP4 - Audio Only': 'audio/mp4; codecs="mp4a.40.2"',
  'MP4 - Video Only': 'video/mp4; codecs="avc1.64001E"',
  // DolbyVision Profile 5
  'MP4 - Video Only': 'video/mp4; codecs="dvh1.05.06"',
  // DolbyVision Profile 8.1 and 8.4
  'MP4 - Video Only': 'video/mp4; codecs="dvhe.08.07"',
  'MP4 - Audio Video': 'video/mp4; codecs="mp4a.40.2, avc1.64001E"'
};

// Global document elements ID's.
var VIDEO_ELEMENT_ID = 'video';
var MEDIA_FILE_ELEMENT_ID = 'mediaFile';
var LICENSE_SERVER_ELEMENT_ID = 'licenseServer';
var KEYSYSTEM_ELEMENT_ID = 'keySystemList';
var MEDIA_TYPE_ELEMENT_ID = 'mediaTypeList';
var USE_MSE_ELEMENT_ID = 'useMSE';
var USE_PLAY_COUNT_ELEMENT_ID = 'playCount';

// These variables get updated every second, so better to have global pointers.
var decodedFPSElement = document.getElementById('decodedFPS');
var droppedFPSElement = document.getElementById('droppedFPS');
var droppedFramesElement = document.getElementById('droppedFrames');
var docLogs = document.getElementById('logs');

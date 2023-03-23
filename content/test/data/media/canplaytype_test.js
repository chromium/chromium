// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// List of unsupported MPEG codec types.
const BAD_MPEG_CODEC_LIST = [
  // AVC codecs must be followed by valid 6-digit hexadecimal number.
  'avc1.12345',
  'avc3.12345',
  'avc1.1234567',
  'avc3.1234567',
  'avc1.number',
  'avc3.number',
  'avc1.12345.',
  'avc3.12345.',
  'avc1.123456.',
  'avc3.123456.',
  'avc1.123456.7',
  'avc3.123456.7',
  'avc1.x23456',
  'avc1.1x3456',
  'avc1.12x456',
  'avc1.123x56',
  'avc1.1234x6',
  'avc1.12345x',

  // Old-style avc1 codecs must be followed by two dot-separated decimal
  // numbers (H.264 profile and level)
  // Invalid formats
  'avc1..',
  'avc1.66.',
  'avc1.66.30.',
  'avc1.x66.30',
  'avc1.66x.30',
  'avc1.66.x30',
  'avc1.66.30x',

  // Invalid level values
  'avc1.66.300',
  'avc1.66.-1',
  'avc1.66.x',

  // Invalid profile values
  'avc1.0.30',
  'avc1.65.30',
  'avc1.67.30',
  'avc1.76.30',
  'avc1.78.30',
  'avc1.99.30',
  'avc1.101.30',
  'avc1.300.30',
  'avc1.-1.30',
  'avc1.x.30',

  // Old-style codec ids are not supported for avc3 codec strings.
  'avc3.66.13',
  'avc3.77.30',
  'avc3.100.40',

  // AAC codecs must be followed by one or two valid hexadecimal numbers.
  'mp4a.no',
  'mp4a.0k',
  'mp4a.0k.0k',
  'mp4a.4.',
  'mp4a.40.0k',
  'mp4a.40.',
  'mp4a.40k',
  'mp4a.40.2k',
  'mp4a.40.2k',
  'mp4a.40.2.',
  'mp4a.40.2.0',

  // Unlike just "avc1", just "mp4a" is not supported.
  'mp4a',
  'mp4a.',

  // Other names for the codecs are not supported.
  'h264',
  'h.264',
  'H264',
  'H.264',
  'aac',
  'AAC',

  // Codecs must not end with a dot.
  'avc1.',
  'avc3.',
  'mp4a.',
  'mp4a.40.',

  // A simple substring match is not sufficient.
  'lavc1337',
  ';mp4a+',
  ';mp4a.40+',

  // Codecs not belonging to MPEG container.
  '1',
  'avc1, 1',
  'avc3, 1',
  'avc1.4D401E, 1',
  'avc3.64001F, 1',
  'vorbis',
  'avc1, vorbis',
  'avc3, vorbis',
  'avc1.4D401E, vorbis',
  'avc3.64001F, vorbis',
  'vp8',
  'vp9',
  'vp8.0',
  'vp9.0',
  'vp08',
  'vp09',
  'vp08.00.01.08.02.01.01.00',
  'vp8, mp4a.40',
  'vp9, mp4a.40',
  'vp8, mp4a.40.2',
  'vp9, mp4a.40.2',
  'vp8, mp4a.40.02',
  'vp9, mp4a.40.02',
  'theora',
  'theora, mp4a',
  'theora, mp4a.40.2',
  'theora, mp4a.40.02',

  // Codecs are case sensitive.
  'AVC1',
  'AVC1.4d401e',
  'AVC3',
  'AVC3.64001f',
  'MP4A',
  'MP4A.40.2',
  'MP4A.40.02',
  'AVC1, MP4',
  'AVC3, MP4',
  ', AVC1.4D401E, MP4.40.2',
  ', AVC3.64001F, MP4.40.2',
  ', AVC1.4D401E, MP4.40.02',
  ', AVC3.64001F, MP4.40.02',

  // Unknown codecs.
  'avc2',
  'avc4',
  'avc1x',
  'avc3x',
  'mp4ax',
  'ac',
  'ec',
  'ac-',
  'ec-',
  'ac-2',
  'ec-2',
  'ac3',
  'ec3',
  'ac-4',
  'ec-4',
  'mp4a.a4',
  'mp4a.a7',
  'mp4a.a5.',
  'mp4a.a6.',
  'mp4a.a5.1',
  'mp4a.a6.1',

  'unknown',

  // Don't allow incomplete/ambiguous codec ids for HEVC.
  // Codec string must have info about codec level/profile, as described in
  // ISO/IEC FDIS 14496-15 section E.3, for example "hev1.1.6.L93.B0"
  'hev1',
  'hvc1',

  // Invalid codecs that look like something similar to HEVC/H.265
  'hev1x',
  'hvc1x',

  // First component of codec id must be "hev1" or "hvc1" (case-sensitive)
  'hevc.1.6.L93.B0',
  'hev0.1.6.L93.B0',
  'hvc0.1.6.L93.B0',
  'hev2.1.6.L93.B0',
  'hvc2.1.6.L93.B0',
  'HEVC.1.6.L93.B0',
  'HEV0.1.6.L93.B0',
  'HVC0.1.6.L93.B0',
  'HEV2.1.6.L93.B0',
  'HVC2.1.6.L93.B0',

  // Trailing dot is not allowed.
  'hev1.1.6.L93.B0.',
  'hvc1.1.6.L93.B0.',

  // Invalid general_profile_space/general_profile_idc
  'hev1.x.6.L93.B0',
  'hvc1.x.6.L93.B0',
  'hev1.d1.6.L93.B0',
  'hvc1.d1.6.L93.B0',

  // Invalid general_profile_compatibility_flags
  'hev1.1.x.L93.B0',
  'hvc1.1.x.L93.B0',

  // Invalid general_tier_flag
  'hev1.1.6.x.B0',
  'hvc1.1.6.x.B0',
  'hev1.1.6.Lx.B0',
  'hvc1.1.6.Lx.B0',
  'hev1.1.6.Hx.B0',
  'hvc1.1.6.Hx.B0',

  // Invalid constraint flags
  'hev1.1.6.L93.x',
  'hvc1.1.6.L93.x',
];

// Codecs not belonging to OGG container.
const BAD_OGG_CODEC_LIST = [
  '1',
  'theora, 1',
  'vp08',
  'vp08.00.01.08.02.01.01.00',
  'vp9',
  'vp9.0',
  'vp9, opus',
  'vp9, vorbis',
  'vp09',
  'vp09.00.10.08',
  'avc1',
  'avc3',
  'avc1.4D401E',
  'avc3.64001F',
  'avc1.66.30',
  'avc1, vorbis',
  'avc3, vorbis',
  'avc1, opus',
  'avc3, opus',
  'hev1.1.6.L93.B0',
  'hvc1.1.6.L93.B0',
  'hev1.1.6.L93.B0,opus',
  'hvc1.1.6.L93.B0,opus',
  'mp3',
  'mp4a.66',
  'mp4a.67',
  'mp4a.68',
  'mp4a.69',
  'mp4a.6B',
  'mp4a.40',
  'mp4a.40.2',
  'mp4a.40.02',
  'theora, mp4a.40.2',
  'theora, mp4a.40.02',
  'ac-3',
  'ec-3',
  'mp4a.A5',
  'mp4a.A6',
  'mp4a.a5',
  'mp4a.a6',

  // Codecs are case sensitive.
  'Theora',
  'OpuS',
  'Vorbis',
  'Theora, OpuS',
  'Theora, Vorbis',

  // Unknown codecs.
  'unknown',
];

// Codecs not belonging to WEBM container.
const BAD_WEBM_CODEC_LIST = [
  '1',
  'vp8, 1',
  'vp9, 1',
  'vp8.0, 1',
  'vp9.0, 1',
  'vp08',
  'vp09',
  'theora',
  'theora, vorbis',
  'theora, opus',
  'avc1',
  'avc3',
  'avc1.4D401E',
  'avc3.64001F',
  'avc1.66.30',
  'avc1, vorbis',
  'avc3, vorbis',
  'avc1, opus',
  'avc3, opus',
  'hev1.1.6.L93.B0',
  'hvc1.1.6.L93.B0',
  'hev1.1.6.L93.B0,opus',
  'hvc1.1.6.L93.B0,opus',
  'flac',
  'mp3',
  'mp4a.66',
  'mp4a.67',
  'mp4a.68',
  'mp4a.69',
  'mp4a.6B',
  'mp4a.40',
  'mp4a.40.2',
  'mp4a.40.02',
  'vp8, mp4a.40',
  'vp9, mp4a.40',
  'vp8.0, mp4a.40',
  'vp9.0, mp4a.40',
  'ac-3',
  'ec-3',
  'mp4a.A5',
  'mp4a.A6',
  'mp4a.a5',
  'mp4a.a6',

  // Codecs are case sensitive.
  'VP8, Vorbis',
  'VP8.0, OpuS',
  'VP9, Vorbis',
  'VP9.0, OpuS',

  // Unknown codec.
  'unknown',
];

// Codecs not belonging to WAV container.
const BAD_WAV_CODEC_LIST = [
  'vp8',
  'vp9',
  'vp8.0, 1',
  'vp9.0, 1',
  'vp08',
  'vp09',
  'vp09.00.10.08',
  'vorbis',
  'opus',
  'theora',
  'theora, 1',
  'avc1',
  'avc3',
  'avc1.4D401E',
  'avc3.64001F',
  'avc1.66.30',
  'avc1, 1',
  'avc3, 1',
  'hev1.1.6.L93.B0',
  'hvc1.1.6.L93.B0',
  'hev1.1.6.L93.B0,opus',
  'hvc1.1.6.L93.B0,opus',
  'flac',
  'mp3',
  'mp4a.66',
  'mp4a.67',
  'mp4a.68',
  'mp4a.69',
  'mp4a.6B',
  'mp4a.40',
  'mp4a.40.2',
  'mp4a.40.02',
  'ac-3',
  'ec-3',
  'mp4a.A5',
  'mp4a.A6',
  'mp4a.a5',
  'mp4a.a6',

  // Unknown codec.
  'unknown',
];

const HLS_CODEC_MAP = {
  'probably': [
    'avc1.42E01E',
    'avc1.42101E',
    'avc1.42701E',
    'avc1.42F01E',
    'avc3.42E01E',
    'avc3.42801E',
    'avc3.42C01E',
    'mp4a.69',
    'mp4a.6B',
    'mp4a.40.2',
    'mp4a.40.02',
    'avc1.42E01E, mp4a.40.2',
    'avc1.42E01E, mp4a.40.02',
    'avc3.42E01E, mp4a.40.5',
    'avc3.42E01E, mp4a.40.05',
    'avc3.42E01E, mp4a.40.29',

    // This result is incorrect. See https://crbug.com/592889.
    'mp3',
  ],

  'maybe': [
    '',
    'avc1',
    'avc3',
    'mp4a.40',
    'avc1, mp4a.40',
    'avc3, mp4a.40',

    'avc1, mp4a.40.2',
    'avc1, mp4a.40.02',
    'avc3, mp4a.40.2',
    'avc3, mp4a.40.02',
    'avc1.42E01E, mp4a.40',
    'avc3.42E01E, mp4a.40',
  ],

  'not': [
    // Android, is the only platform that supports these types, and its HLS
    // implementations uses platform codecs, which do not include MPEG-2 AAC.
    // See https://crbug.com/544268.
    'mp4a.66',
    'mp4a.67',
    'mp4a.68',
    'hev1.1.6.L93.B0',
    'hvc1.1.6.L93.B0',
    'hev1.1.6.L93.B0,mp4a.40.5',
    'hvc1.1.6.L93.B0,mp4a.40.5',
    'vp09.00.10.08',
    'flac',
    'ac-3',
    'ec-3',
    'mp4a.A5',
    'mp4a.A6',
    'mp4a.a5',
    'mp4a.a6',
  ]
};

const video = document.querySelector('video');

function testMimeCodec(mime_codec, expected_value) {
  let result = video.canPlayType(mime_codec);
  if (expected_value === result)
    return true;
  console.log(
      `Expected "${expected_value}" got "${result}" for ` +
      `canPlayType('${mime_codec}')`);
  return false;
}

function testCodecList(mime, codec_list, expected_value) {
  for (var i = 0; i < codec_list.length; ++i) {
    let codec = codec_list[i];
    if (!testMimeCodec(`${mime}; codecs="${codec}"`, expected_value))
      return false;
  }

  return true;
}

function testMimeCodecList(mime_codec_list, expected_value) {
  let video = document.querySelector('video');
  for (var i = 0; i < mime_codec_list.length; ++i) {
    let mime_and_codec = mime_codec_list[i];
    if (!testMimeCodec(mime_and_codec, expected_value))
      return false;
  }
  return true;
}

function testMimeCodecMap(mime_codec_map, expect_not) {
  let probably_list = mime_codec_map['probably'];
  if (!testMimeCodecList(probably_list, expect_not ? '' : 'probably'))
    return false;

  // Not all codecs have a maybe value.
  if ('maybe' in mime_codec_map) {
    let maybe_list = mime_codec_map['maybe'];
    if (!testMimeCodecList(maybe_list, expect_not ? '' : 'maybe'))
      return false;
  }

  if ('not' in mime_codec_map)
    return testMimeCodecList(mime_codec_map['not'], '');

  return true;
}

function testBadMpegVariants(mime) {
  let codecs_to_test = [...BAD_MPEG_CODEC_LIST];

  // Old-style avc1 codec ids are supported only for video/mp2t container.
  if (mime != 'video/mp2t') {
    codecs_to_test.push('avc1.66.13');
    codecs_to_test.push('avc1.77.30');
    codecs_to_test.push('avc1.100.40');
  }

  // Remove all but "audio/mpeg" when https://crbug.com/592889 is fixed.
  if (mime != 'audio/mpeg' && mime != 'audio/mp4' && mime != 'video/mp4' &&
      mime != 'video/mp2t' && !mime.endsWith('mpegurl')) {
    codecs_to_test.push('mp3')
  }

  if (mime != 'audio/mp4' && mime != 'video/mp4') {
    codecs_to_test.push('opus');
    codecs_to_test.push('avc1, opus');
    codecs_to_test.push('avc3, opus');
    codecs_to_test.push('avc1.4D401E, opus');
    codecs_to_test.push('avc3.64001F, opus');
  }

  return testCodecList(mime, codecs_to_test, '');
}

function testBadOggVariants(mime) {
  return testCodecList(mime, BAD_OGG_CODEC_LIST, '');
}

function testBadWebmVariants(mime) {
  return testCodecList(mime, BAD_WEBM_CODEC_LIST, '');
}

function testBadWavVariants(mime) {
  return testCodecList(mime, BAD_WAV_CODEC_LIST, '');
}

function testHlsVariants(mime, has_hls_support) {
  let probably_list = HLS_CODEC_MAP['probably'];
  if (!testCodecList(mime, probably_list, has_hls_support ? 'probably' : ''))
    return false;

  let maybe_list = HLS_CODEC_MAP['maybe'];
  if (!testCodecList(mime, maybe_list, has_hls_support ? 'maybe' : ''))
    return false;

  if (!testBadMpegVariants(mime))
    return false;

  return testCodecList(mime, HLS_CODEC_MAP['not'], '');
}

function testAv1Variants(has_av1_support) {
  const AV1_CODEC_MAP = {
    'probably': [
      // Fully qualified codec strings are required. These tests are not
      // exhaustive since codec string parsing is exhaustively tested elsewhere.
      'video/webm; codecs="av01.0.04M.08"',
      'video/mp4; codecs="av01.0.04M.08"',
    ],
    'not': [
      'video/webm; codecs="av1"',
      'video/mp4; codecs="av1"',
    ],
  }

  return testMimeCodecMap(AV1_CODEC_MAP, !has_av1_support);
}

function testWavVariants() {
  const WAV_CODEC_MAP = {
    'probably': [
      'audio/wav; codecs="1"',
      'audio/x-wav; codecs="1"',
    ],
    'maybe': [
      'audio/wav',
      'audio/x-wav',
    ],
  };

  return testBadWavVariants('audio/wav') && testBadWavVariants('audio/x-wav') &&
      testMimeCodecMap(WAV_CODEC_MAP, false);
}

function testWebmVariants() {
  const WEBM_CODEC_MAP = {
    'probably': [
      'video/webm; codecs="vp8"',
      'video/webm; codecs="vp8.0"',
      'video/webm; codecs="vp8, vorbis"',
      'video/webm; codecs="vp8.0, vorbis"',
      'video/webm; codecs="vp8, opus"',
      'video/webm; codecs="vp8.0, opus"',
      'video/webm; codecs="vp9"',
      'video/webm; codecs="vp9.0"',
      'video/webm; codecs="vp9, vorbis"',
      'video/webm; codecs="vp9.0, vorbis"',
      'video/webm; codecs="vp9, opus"',
      'video/webm; codecs="vp9.0, opus"',
      'video/webm; codecs="vp8, vp9"',
      'video/webm; codecs="vp8.0, vp9.0"',
      'audio/webm; codecs="vorbis"',
      'audio/webm; codecs="opus"',
      'audio/webm; codecs="opus, vorbis"',
    ],
    'maybe': [
      'video/webm',
      'audio/webm',
    ],
    'not': [
      'audio/webm; codecs="vp8"',
      'audio/webm; codecs="vp8.0"',
      'audio/webm; codecs="vp8, vorbis"',
      'audio/webm; codecs="vp8.0, vorbis"',
      'audio/webm; codecs="vp8, opus"',
      'audio/webm; codecs="vp8.0, opus"',
      'audio/webm; codecs="vp9"',
      'audio/webm; codecs="vp9.0"',
      'audio/webm; codecs="vp9, vorbis"',
      'audio/webm; codecs="vp9.0, vorbis"',
      'audio/webm; codecs="vp9, opus"',
      'audio/webm; codecs="vp9.0, opus"',
    ],
  };

  return testBadWebmVariants('video/webm') &&
      testBadWebmVariants('audio/webm') &&
      testMimeCodecMap(WEBM_CODEC_MAP, false);
}

function testOggVariants(has_theora_support) {
  const OGG_THEORA_MAP = {
    'probably': [
      'video/ogg; codecs="theora"',
      'video/ogg; codecs="theora, flac"',
      'video/ogg; codecs="theora, opus"',
      'video/ogg; codecs="theora, vorbis"',
      'application/ogg; codecs="theora"',
      'application/ogg; codecs="theora, flac"',
      'application/ogg; codecs="theora, opus"',
      'application/ogg; codecs="theora, vorbis"',
    ],
    'not': [
      'audio/ogg; codecs="theora"',
      'audio/ogg; codecs="theora, flac"',
      'audio/ogg; codecs="theora, opus"',
      'audio/ogg; codecs="theora, vorbis"',
    ],
  };

  const OGG_CODEC_MAP = {
    'probably': [
      'video/ogg; codecs="flac, opus, vorbis"',
      'video/ogg; codecs="vp8"',
      'video/ogg; codecs="vp8.0"',
      'video/ogg; codecs="vp8, opus"',
      'video/ogg; codecs="vp8, vorbis"',
      'audio/ogg; codecs="flac"',
      'audio/ogg; codecs="opus"',
      'audio/ogg; codecs="vorbis"',
      'audio/ogg; codecs="flac, vorbis, opus"',
      'application/ogg; codecs="flac"',
      'application/ogg; codecs="opus"',
      'application/ogg; codecs="vorbis"',
      'application/ogg; codecs="flac, opus, vorbis"',
    ],
    'maybe': [
      'application/ogg',
      'video/ogg',
      'audio/ogg',
    ]
  };
  return testBadOggVariants('video/ogg') && testBadOggVariants('audio/ogg') &&
      testBadOggVariants('application/ogg') &&
      testMimeCodecMap(OGG_THEORA_MAP, !has_theora_support) &&
      testMimeCodecMap(OGG_CODEC_MAP, false);
}

function testFlacVariants() {
  const FLAC_CODEC_MAP = {
    'probably': [
      'audio/flac',
      'audio/ogg; codecs="flac"',
      'audio/ogg; codecs="fLaC"',

      // See CodecSupportTest_mp4 for more flac Variants.
      'audio/mp4; codecs="flac"',
      'video/mp4; codecs="flac"',
      'audio/mp4; codecs="fLaC"',
      'video/mp4; codecs="fLaC"',
    ],
    'not': [
      'video/flac',
      'video/x-flac',
      'audio/x-flac',
      'application/x-flac',
      'audio/flac; codecs="flac"',
      'video/webm; codecs="flac"',
      'audio/webm; codecs="flac"',
      'audio/flac; codecs="avc1"',
      'audio/flac; codecs="avc3"',
      'audio/flac; codecs="avc1.4D401E"',
      'audio/flac; codecs="avc3.64001F"',
      'audio/flac; codecs="mp4a.66"',
      'audio/flac; codecs="mp4a.67"',
      'audio/flac; codecs="mp4a.68"',
      'audio/flac; codecs="mp4a.40.2"',
      'audio/flac; codecs="mp4a.40.02"',
    ],
  };

  return testMimeCodecMap(FLAC_CODEC_MAP, false);
}

function testMp3Variants() {
  const MP3_CODEC_MAP = {
    'probably': [
      // audio/mpeg without a codecs parameter (RFC 3003 compliant)
      'audio/mpeg',

      // audio/mpeg with mp3 in codecs parameter. (Not RFC compliant, but
      // very common in the wild so it is a defacto standard).
      'audio/mpeg; codecs="mp3"',
      // The next two results are wrong due to https://crbug.com/592889.
      'audio/mpeg; codecs="mp4a.69"',
      'audio/mpeg; codecs="mp4a.6B"',

      // audio/mp3 does not allow any codecs parameter
      'audio/mp3',

      // audio/x-mp3 does not allow any codecs parameter
      'audio/x-mp3',
    ],
    'not': [
      'video/mp3',
      'video/mpeg',
      'video/x-mp3',
      'audio/mp3; codecs="mp3"',
      'audio/x-mp3; codecs="mp3"',
    ],
  };

  const MP3_BAD_CODEC_LIST = [
    'avc1',
    'avc3',
    'avc1.4D401E',
    'avc3.64001F',
    'mp4a.66',
    'mp4a.67',
    'mp4a.68',
    'mp4a.40.2',
    'mp4a.40.02',
    'flac',
  ];

  return testCodecList('audio/mp3', MP3_BAD_CODEC_LIST, '') &&
      testCodecList('audio/mpeg', MP3_BAD_CODEC_LIST, '') &&
      testCodecList('audio/x-mp3', MP3_BAD_CODEC_LIST, '') &&
      testBadMpegVariants('audio/mp3') && testBadMpegVariants('audio/mpeg') &&
      testBadMpegVariants('audio/x-mp3') &&
      testMimeCodecMap(MP3_CODEC_MAP, false);
}

function testMp4Variants(
    has_proprietary_codecs, platform_guarantees_hevc,
    platform_guarantees_ac3_eac3) {
  const MP4_CODEC_MAP = {
    'probably': [
      'audio/mp4; codecs="flac"',
      'audio/mp4; codecs="fLaC"',
      'audio/mp4; codecs="mp4a.69"',
      'audio/mp4; codecs="mp4a.6B"',
      'audio/mp4; codecs="opus"',
      'audio/mp4; codecs="Opus"',
      'video/mp4; codecs="flac"',
      'video/mp4; codecs="fLaC"',
      'video/mp4; codecs="mp4a.69"',
      'video/mp4; codecs="mp4a.6B"',
      'video/mp4; codecs="opus"',
      'video/mp4; codecs="vp09.00.10.08"',

      // These results are incorrect. See https://crbug.com/592889.
      'video/mp4; codecs="mp3"',
      'audio/mp4; codecs="mp3"',
    ],
    'maybe': [
      'audio/mp4',
      'video/mp4',
    ],
    'not': [
      'video/x-m4a; codecs="flac"',
      'video/x-m4a; codecs="opus"',
      'video/x-m4a; codecs="mp4a.69"',
      'video/x-m4a; codecs="mp4a.6B"',
      'video/x-m4v; codecs="ac-3"',
      'video/x-m4v; codecs="avc1.640028,ac-3"',
      'video/x-m4v; codecs="avc1.640028,ec-3"',
      'video/x-m4v; codecs="avc1.640028,mp4a.A5"',
      'video/x-m4v; codecs="avc1.640028,mp4a.A6"',
      'video/x-m4v; codecs="avc1.640028,mp4a.a5"',
      'video/x-m4v; codecs="avc1.640028,mp4a.a6"',
      'video/x-m4v; codecs="ec-3"',
      'video/x-m4v; codecs="flac"',
      'video/x-m4v; codecs="mp4a.69"',
      'video/x-m4v; codecs="mp4a.6B"',
      'video/x-m4v; codecs="mp4a.A5"',
      'video/x-m4v; codecs="mp4a.A6"',
      'video/x-m4v; codecs="mp4a.a5"',
      'video/x-m4v; codecs="mp4a.a6"',
      'video/x-m4v; codecs="opus"',
      'video/x-m4v; codecs="vp09.00.10.08"',
      'video/x-m4v; codecs="hev1.1.6.L93.B0, mp4a.40.5"',
      'video/x-m4v; codecs="hev1.1.6.L93.B0"',
      'video/x-m4v; codecs="hvc1.1.6.L93.B0, mp4a.40.5"',
      'video/x-m4v; codecs="hvc1.1.6.L93.B0"',
    ],
  };

  if (platform_guarantees_hevc) {
    MP4_CODEC_MAP['probably'] = MP4_CODEC_MAP['probably'].concat([
      'video/mp4; codecs="hev1.1.6.L93.B0, mp4a.40.5"',
      'video/mp4; codecs="hev1.1.6.L93.B0"',
      'video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.5"',
      'video/mp4; codecs="hvc1.1.6.L93.B0"'
    ])
  } else {
    MP4_CODEC_MAP['not'] = MP4_CODEC_MAP['not'].concat([
      'video/mp4; codecs="hev1.1.6.L93.B0, mp4a.40.5"',
      'video/mp4; codecs="hev1.1.6.L93.B0"',
      'video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.5"',
      'video/mp4; codecs="hvc1.1.6.L93.B0"'
    ])
  }

  // AC3 and EAC3 (aka Dolby Digital Plus, DD+) audio codecs.
  // TODO(servolk): Strictly speaking only
  // mp4a.A5 and mp4a.A6 codec ids are valid according to RFC 6381 section
  // 3.3, 3.4. Lower-case oti (mp4a.a5 and mp4a.a6) should be rejected. But
  // we used to allow those in older versions of Chromecast firmware and
  // some apps (notably MPL) depend on those codec types being supported, so
  // they should be allowed for now (crbug.com/564960)
  let ac3_eac3_codecs = [
    'video/mp4; codecs="ac-3"',
    'video/mp4; codecs="mp4a.a5"',
    'video/mp4; codecs="mp4a.A5"',
    'video/mp4; codecs="ec-3"',
    'video/mp4; codecs="mp4a.a6"',
    'video/mp4; codecs="mp4a.A6"',
    'video/mp4; codecs="avc1.640028,ac-3"',
    'video/mp4; codecs="avc1.640028,mp4a.a5"',
    'video/mp4; codecs="avc1.640028,mp4a.A5"',
    'video/mp4; codecs="avc1.640028,ec-3"',
    'video/mp4; codecs="avc1.640028,mp4a.a6"',
    'video/mp4; codecs="avc1.640028,mp4a.A6"',
  ];
  if (platform_guarantees_ac3_eac3) {
    MP4_CODEC_MAP['probably'] =
        MP4_CODEC_MAP['probably'].concat(ac3_eac3_codecs);
  } else {
    MP4_CODEC_MAP['not'] = MP4_CODEC_MAP['not'].concat(ac3_eac3_codecs);
  }

  const MP4A_BAD_CODEC_LIST = [
    'avc1, mp4a.40',
    'avc1, mp4a',
    'avc1.4D401E',
    'avc1',
    'avc3, mp4a.40',
    'avc3, mp4a',
    'avc3.64001F',
    'avc3',
    'hev1.1.6.L93.B0,mp4a.40.5',
    'hev1.1.6.L93.B0',
    'hvc1.1.6.L93.B0,mp4a.40.5',
    'hvc1.1.6.L93.B0',
    'vp09.00.10.08',
  ];

  const MP4_PROP_CODEC_MAP = {
    'probably': [
      'audio/mp4; codecs="mp4a.40.02"',
      'audio/mp4; codecs="mp4a.40.05"',
      'audio/mp4; codecs="mp4a.40.29"',
      'audio/mp4; codecs="mp4a.40.2"',
      'audio/mp4; codecs="mp4a.40.5"',
      'audio/mp4; codecs="mp4a.66"',
      'audio/mp4; codecs="mp4a.67"',
      'audio/mp4; codecs="mp4a.68"',
      'audio/x-m4a; codecs="mp4a.40.02"',
      'audio/x-m4a; codecs="mp4a.40.05"',
      'audio/x-m4a; codecs="mp4a.40.29"',
      'audio/x-m4a; codecs="mp4a.40.2"',
      'audio/x-m4a; codecs="mp4a.40.5"',
      'audio/x-m4a; codecs="mp4a.66"',
      'audio/x-m4a; codecs="mp4a.67"',
      'audio/x-m4a; codecs="mp4a.68"',
      'video/mp4; codecs="avc1.42101E"',
      'video/mp4; codecs="avc1.42701E"',
      'video/mp4; codecs="avc1.42E01E, mp4a.40.02"',
      'video/mp4; codecs="avc1.42E01E, mp4a.40.2"',
      'video/mp4; codecs="avc1.42E01E"',
      'video/mp4; codecs="avc1.42F01E"',
      'video/mp4; codecs="avc1.4D401E, flac"',
      'video/mp4; codecs="avc1.4D401E, opus"',
      'video/mp4; codecs="avc3.42801E"',
      'video/mp4; codecs="avc3.42C01E"',
      'video/mp4; codecs="avc3.42E01E, mp4a.40.05"',
      'video/mp4; codecs="avc3.42E01E, mp4a.40.29"',
      'video/mp4; codecs="avc3.42E01E, mp4a.40.5"',
      'video/mp4; codecs="avc3.42E01E"',
      'video/mp4; codecs="avc3.64001F, flac"',
      'video/mp4; codecs="avc3.64001F, opus"',
      'video/mp4; codecs="mp4a.40.02"',
      'video/mp4; codecs="mp4a.40.2"',
      'video/mp4; codecs="mp4a.66"',
      'video/mp4; codecs="mp4a.67"',
      'video/mp4; codecs="mp4a.68"',
      'video/x-m4v; codecs="avc1.42101E"',
      'video/x-m4v; codecs="avc1.42701E"',
      'video/x-m4v; codecs="avc1.42E01E, mp4a.40.02"',
      'video/x-m4v; codecs="avc1.42E01E, mp4a.40.2"',
      'video/x-m4v; codecs="avc1.42E01E"',
      'video/x-m4v; codecs="avc1.42F01E"',
      'video/x-m4v; codecs="avc3.42801E"',
      'video/x-m4v; codecs="avc3.42C01E"',
      'video/x-m4v; codecs="avc3.42E01E, mp4a.40.05"',
      'video/x-m4v; codecs="avc3.42E01E, mp4a.40.29"',
      'video/x-m4v; codecs="avc3.42E01E, mp4a.40.5"',
      'video/x-m4v; codecs="avc3.42E01E"',
      'video/x-m4v; codecs="mp4a.40.02"',
      'video/x-m4v; codecs="mp4a.40.2"',
      'video/x-m4v; codecs="mp4a.66"',
      'video/x-m4v; codecs="mp4a.67"',
      'video/x-m4v; codecs="mp4a.68"',
    ],
    'maybe': [
      'audio/mp4; codecs="mp4a.40"',
      'audio/x-m4a',
      'audio/x-m4a; codecs="mp4a.40"',
      'video/mp4; codecs="avc1, avc3"',
      'video/mp4; codecs="avc1, flac"',
      'video/mp4; codecs="avc1, mp4a.40.02"',
      'video/mp4; codecs="avc1, mp4a.40.2"',
      'video/mp4; codecs="avc1, mp4a.40"',
      'video/mp4; codecs="avc1, opus"',
      'video/mp4; codecs="avc1.42E01E, mp4a.40"',
      'video/mp4; codecs="avc1"',
      'video/mp4; codecs="avc3, flac"',
      'video/mp4; codecs="avc3, mp4a.40.02"',
      'video/mp4; codecs="avc3, mp4a.40.2"',
      'video/mp4; codecs="avc3, mp4a.40"',
      'video/mp4; codecs="avc3, opus"',
      'video/mp4; codecs="avc3.42E01E, mp4a.40"',
      'video/mp4; codecs="avc3"',
      'video/mp4; codecs="mp4a.40"',
      'video/x-m4v',
      'video/x-m4v; codecs="avc1, avc3"',
      'video/x-m4v; codecs="avc1, mp4a.40.02"',
      'video/x-m4v; codecs="avc1, mp4a.40.2"',
      'video/x-m4v; codecs="avc1, mp4a.40"',
      'video/x-m4v; codecs="avc1.42E01E, mp4a.40"',
      'video/x-m4v; codecs="avc1"',
      'video/x-m4v; codecs="avc3, mp4a.40.02"',
      'video/x-m4v; codecs="avc3, mp4a.40.2"',
      'video/x-m4v; codecs="avc3, mp4a.40"',
      'video/x-m4v; codecs="avc3.42E01E, mp4a.40"',
      'video/x-m4v; codecs="avc3"',
      'video/x-m4v; codecs="mp4a.40"',
    ],
    'not': [
      'video/x-m4a; codecs="flac"',
      'video/x-m4a; codecs="opus"',
      'video/x-m4a; codecs="mp4a.69"',
      'video/x-m4a; codecs="mp4a.6B"',
      'video/x-m4v; codecs="ac-3"',
      'video/x-m4v; codecs="avc1.640028,ac-3"',
      'video/x-m4v; codecs="avc1.640028,ec-3"',
      'video/x-m4v; codecs="avc1.640028,mp4a.A5"',
      'video/x-m4v; codecs="avc1.640028,mp4a.A6"',
      'video/x-m4v; codecs="avc1.640028,mp4a.a5"',
      'video/x-m4v; codecs="avc1.640028,mp4a.a6"',
      'video/x-m4v; codecs="ec-3"',
      'video/x-m4v; codecs="flac"',
      'video/x-m4v; codecs="mp4a.69"',
      'video/x-m4v; codecs="mp4a.6B"',
      'video/x-m4v; codecs="mp4a.A5"',
      'video/x-m4v; codecs="mp4a.A6"',
      'video/x-m4v; codecs="mp4a.a5"',
      'video/x-m4v; codecs="mp4a.a6"',
      'video/x-m4v; codecs="opus"',
      'video/x-m4v; codecs="vp09.00.10.08"',
    ],
  };

  return testCodecList('audio/mp4', MP4A_BAD_CODEC_LIST, '') &&
      testCodecList('audio/x-m4a', MP4A_BAD_CODEC_LIST, '') &&
      testMimeCodecMap(MP4_CODEC_MAP, false) &&
      testMimeCodecMap(MP4_PROP_CODEC_MAP, !has_proprietary_codecs) &&
      testBadMpegVariants('video/mp4') && testBadMpegVariants('video/x-m4v') &&
      testBadMpegVariants('audio/mp4') && testBadMpegVariants('audio/x-m4a');
}

function testAvcVariantsInternal(
    has_proprietary_codecs, has_software_avc, avc) {
  let P_MAYBE = has_proprietary_codecs ? 'maybe' : '';
  let P_PROBABLY = has_proprietary_codecs ? 'probably' : '';

  // avc1 without extensions results in "maybe" for compatibility.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '"', P_MAYBE))
    return false;

  // A valid-looking 6-digit hexadecimal number will result in at least "maybe".
  // But the first hex byte after the dot must be a valid profile_idc and the
  // lower two bits of the second byte/4th digit must be zero.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42AC23"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42ACDF"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42acdf"', P_MAYBE))
    return false;

  // Invalid profile 0x12.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.123456"', ''))
    return false;
  // Valid profile/level, but reserved bits are set to 1 (4th digit after dot).
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42011E"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42021E"', ''))
    return false;

  // Both upper and lower case hexadecimal digits are accepted.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42E01E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42e01e"', P_PROBABLY))
    return false;

  // From a YouTube DASH MSE test manifest.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4d401f"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4d401e"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4d4015"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.640028"', P_PROBABLY))
    return false;

  //
  // Baseline Profile (66 == 0x42).
  //  The first two digits after the dot must be 42. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42001E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42401E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42801E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42E00A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42G01E"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42000G"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.42E0FF"', P_MAYBE))
    return false;

  //
  // Main Profile (77 == 0x4D).
  //  The first two digits after the dot must be 4D. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4D001E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4D400A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4D800A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4DE00A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4DG01E"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4D000G"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.4DE0FF"', P_MAYBE))
    return false;

  //
  // High Profile (100 == 0x64).
  //  The first two digits after the dot must be 64. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64001E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64400A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64800A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64E00A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64G01E"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64000G"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.64E0FF"', P_MAYBE))
    return false;

  //
  // High 10-bit Profile (110 == 0x6E).
  //  The first two digits after the dot must be 6E. The third and fourth digits
  //  contain constraint_set_flags and must be valid hex. The last two digits
  //  should be any valid H.264 level. If the level value is invalid the result
  //  will be kMaybe.
  //
  let HI10P_PROBABLY =
      has_software_avc ? 'probably' : (has_proprietary_codecs ? 'maybe' : '');
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6E001E"', HI10P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6E400A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6E800A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6EE00A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6EG01E"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6E000G"', ''))
    return false;
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.6EE0FF"', P_MAYBE))
    return false;

  //
  //  Other profiles are not known to be supported.
  //

  // Extended Profile (88 == 0x58).
  //   Without any constraint flags.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.58001E"', P_MAYBE))
    return false;
  //   With constraint_set0_flag==1 indicating compatibility with baseline
  //   profile.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.58801E"', P_PROBABLY))
    return false;
  //   With constraint_set1_flag==1 indicating compatibility with main profile.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.58401E"', P_PROBABLY))
    return false;
  //   With constraint_set2_flag==1 indicating compatibility with extended
  //   profile, the result is 'maybe' the same as for straight extended profile.
  if (!testMimeCodec('video/mp4; codecs="' + avc + '.58201E"', P_MAYBE))
    return false;

  return true;
}

function testAvcVariants(has_proprietary_codecs, has_software_avc) {
  return testAvcVariantsInternal(
             has_proprietary_codecs, has_software_avc, 'avc1') &&
      testAvcVariantsInternal(has_proprietary_codecs, has_software_avc, 'avc3');
}

// Tests AVC levels using |avc|.
// Other supported values for the first four hexadecimal digits should behave
// the same way but are not tested.
// For each full level, the following are tested:
// * The hexadecimal value before it is not supported.
// * The hexadecimal value for the main level and all sub-levels are supported.
// * The hexadecimal value after the last sub-level it is not supported.
// * Decimal representations of the levels are not supported.
function testAvcLevelsInternal(has_proprietary_codecs, avc) {
  let P_MAYBE = has_proprietary_codecs ? 'maybe' : '';
  let P_PROBABLY = has_proprietary_codecs ? 'probably' : '';

  // Level 0 is not supported.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E000"', P_MAYBE))
    return false;

  // Levels 1 (0x0A), 1.1 (0x0B), 1.2 (0x0C), 1.3 (0x0D).
  if (!testMimeCodec('video/mp4; codecs="avc1.42E009"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E00A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E00B"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E00C"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E00D"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E00E"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E001"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E010"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E011"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E012"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E013"', P_MAYBE))
    return false;

  // Levels 2 (0x14), 2.1 (0x15), 2.2 (0x16)
  if (!testMimeCodec('video/mp4; codecs="avc1.42E013"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E014"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E015"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E016"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E017"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  // However, 20 is level 3.2.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E002"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E020"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E021"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E022"', P_MAYBE))
    return false;

  // Levels 3 (0x1e), 3.1 (0x1F), 3.2 (0x20)
  if (!testMimeCodec('video/mp4; codecs="avc1.42E01D"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E01E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E01F"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E020"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E021"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  // However, 32 is level 5.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E003"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E030"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E031"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E032"', P_PROBABLY))
    return false;

  // Levels 4 (0x28), 4.1 (0x29), 4.2 (0x2A)
  if (!testMimeCodec('video/mp4; codecs="avc1.42E027"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E028"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E029"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E02A"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E02B"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E004"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E040"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E041"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E042"', P_MAYBE))
    return false;

  // Levels 5 (0x32), 5.1 (0x33), 5.2 (0x34).
  if (!testMimeCodec('video/mp4; codecs="avc1.42E031"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E032"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E033"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E034"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E035"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E005"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E050"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E051"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E052"', P_MAYBE))
    return false;

  // Levels 6 (0x3C), 6.1 (0x3D), 6.2 (0x3E).
  if (!testMimeCodec('video/mp4; codecs="avc1.42E03B"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E03C"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E03D"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E03E"', P_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E03F"', P_MAYBE))
    return false;
  // Verify that decimal representations of levels are not supported.
  if (!testMimeCodec('video/mp4; codecs="avc1.42E006"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E060"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E061"', P_MAYBE))
    return false;
  if (!testMimeCodec('video/mp4; codecs="avc1.42E062"', P_MAYBE))
    return false;

  return true;
}

function testAvcLevels(has_proprietary_codecs) {
  return testAvcLevelsInternal(has_proprietary_codecs, 'avc1') &&
      testAvcLevelsInternal(has_proprietary_codecs, 'avc3');
}

// All values that return positive results are tested. There are also
// negative tests for values around or that could potentially be confused with
// (e.g. case, truncation, hex <-> decimal conversion) those values that return
// positive results.
function testMp4aVariants(has_proprietary_codecs, has_xhe_aac_support) {
  const MP3_CODEC_LIST = ['mp4a.6B', 'mp4a.69'];
  const MP4A_BAD_CODEC_LIST = [
    'mp4a',
    'mp4a.',
    'mp4a.6',
    'mp4a.60',
    'mp4a.61',
    'mp4a.62',
    'mp4a.63',
    'mp4a.65',
    'mp4a.65',
    'mp4a.6A',
    'mp4a.6b',
    'mp4a.6C',
    'mp4a.6D',
    'mp4a.6E',
    'mp4a.6F',
    'mp4a.76',
    'mp4a.4',
    'mp4a.39',
    'mp4a.40.',
    'mp4a.40.0',
    'mp4a.40.1',
    'mp4a.40.3',
    'mp4a.40.4',
    'mp4a.40.6',
    'mp4a.40.7',
    'mp4a.40.8',
    'mp4a.40.9',
    'mp4a.40.10',
    'mp4a.40.20',
    'mp4a.40.30',
    'mp4a.40.40',
    'mp4a.40.50',
    'mp4a.40.290',

    // Check conversions of decimal 29 to hex and hex 29 to decimal.
    'mp4a.40.1d',
    'mp4a.40.1D',
    'mp4a.40.41',

    // Allow leading zeros in aud-oti for specific MPEG4 AAC strings.
    // See http://crbug.com/440607.
    'mp4a.40.00',
    'mp4a.40.01',
    'mp4a.40.03',
    'mp4a.40.04',
    'mp4a.40.029',
    'mp4a.41',
    'mp4a.41.2',
    'mp4a.4.2',
    'mp4a.400.2',
    'mp4a.040.2',
    'mp4a.4.5',
    'mp4a.400.5',
    'mp4a.040.5',
  ];

  const MP4A_GOOD_CODEC_LIST = [
    // MPEG2 AAC Main, LC, and SSR are supported.
    'mp4a.66',
    'mp4a.67',
    'mp4a.68',

    // MPEG4 AAC SBR PS v2.
    'mp4a.40.29',

    // MPEG4 AAC SBR v1.
    'mp4a.40.05',

    // MPEG4 AAC LC.
    'mp4a.40.2',

    // MPEG4 AAC SBR v1.
    'mp4a.40.5',

    // MPEG4 AAC LC.
    'mp4a.40.02',
  ];

  let P_MAYBE = has_proprietary_codecs ? 'maybe' : '';
  let P_PROBABLY = has_proprietary_codecs ? 'probably' : '';

  // mp4a.40 without further extension is ambiguous and results in "maybe".
  if (!testMimeCodec('audio/mp4; codecs="mp4a.40"', P_MAYBE))
    return false;

  let XHE_PROBABLY = has_xhe_aac_support ? 'probably' : '';
  if (!testMimeCodec('audio/mp4; codecs="mp4a.40.42"', XHE_PROBABLY))
    return false;
  if (!testMimeCodec('video/mp4; codecs="mp4a.40.42"', XHE_PROBABLY))
    return false;

  return testCodecList('audio/mp4', MP3_CODEC_LIST, 'probably') &&
      testCodecList('audio/mp4', MP4A_BAD_CODEC_LIST, '') &&
      testCodecList('audio/mp4', MP4A_GOOD_CODEC_LIST, P_PROBABLY);
}

function testHls(has_hls_support) {
  return testHlsVariants('application/vnd.apple.mpegurl', has_hls_support) &&
      testHlsVariants('application/x-mpegurl', has_hls_support) &&
      testHlsVariants('audio/mpegurl', has_hls_support) &&
      testHlsVariants('audio/x-mpegurl', has_hls_support);
}

function testAacAdts(has_proprietary_codecs) {
  let P_PROBABLY = has_proprietary_codecs ? 'probably' : '';
  if (!testMimeCodec('audio/aac', P_PROBABLY))
    return false;

  // audio/aac doesn't support the codecs parameter.
  const ADTS_BAD_CODEC_LIST = ['1', 'aac', 'mp4a.40.2'];
  return testCodecList('audio/aac', ADTS_BAD_CODEC_LIST, '');
}

function testMp2tsVariants(has_mp2ts_support) {
  const MP2T_CODEC_MAP = {
    'probably': [
      // video/mp2t must support standard RFC 6381 compliant H.264 / AAC codec
      // ids. H.264 baseline, main, high profiles
      'video/mp2t; codecs="avc1.42E01E"',
      'video/mp2t; codecs="avc1.4D401E"',
      'video/mp2t; codecs="avc1.640028"',
      'video/mp2t; codecs="mp4a.66"',
      'video/mp2t; codecs="mp4a.67"',
      'video/mp2t; codecs="mp4a.68"',
      'video/mp2t; codecs="mp4a.69"',
      'video/mp2t; codecs="mp4a.6B"',

      // AAC LC audio
      'video/mp2t; codecs="mp4a.40.2"',

      // H.264 + AAC audio Variants
      'video/mp2t; codecs="avc1.42E01E,mp4a.40.2"',
      'video/mp2t; codecs="avc1.4D401E,mp4a.40.2"',
      'video/mp2t; codecs="avc1.640028,mp4a.40.2"',

      // This result is incorrect. See https://crbug.com/592889.
      'video/mp2t; codecs="mp3"',

      // Old-style avc1/H.264 codec ids that are still being used by some HLS
      // streaming apps for backward compatibility.
      // H.264 baseline profile
      'video/mp2t; codecs="avc1.66.10"',
      'video/mp2t; codecs="avc1.66.13"',
      'video/mp2t; codecs="avc1.66.20"',
      'video/mp2t; codecs="avc1.66.22"',
      'video/mp2t; codecs="avc1.66.30"',
      'video/mp2t; codecs="avc1.66.32"',
      'video/mp2t; codecs="avc1.66.40"',
      'video/mp2t; codecs="avc1.66.42"',

      // H.264 main profile
      'video/mp2t; codecs="avc1.77.10"',
      'video/mp2t; codecs="avc1.77.13"',
      'video/mp2t; codecs="avc1.77.20"',
      'video/mp2t; codecs="avc1.77.22"',
      'video/mp2t; codecs="avc1.77.30"',
      'video/mp2t; codecs="avc1.77.32"',
      'video/mp2t; codecs="avc1.77.40"',
      'video/mp2t; codecs="avc1.77.42"',

      // H.264 high profile
      'video/mp2t; codecs="avc1.100.10"',
      'video/mp2t; codecs="avc1.100.13"',
      'video/mp2t; codecs="avc1.100.20"',
      'video/mp2t; codecs="avc1.100.22"',
      'video/mp2t; codecs="avc1.100.30"',
      'video/mp2t; codecs="avc1.100.32"',
      'video/mp2t; codecs="avc1.100.40"',
      'video/mp2t; codecs="avc1.100.42"',

      // H.264 + AAC audio Variants
      'video/mp2t; codecs="avc1.66.10,mp4a.40.2"',
      'video/mp2t; codecs="avc1.66.30,mp4a.40.2"',
      'video/mp2t; codecs="avc1.77.10,mp4a.40.2"',
      'video/mp2t; codecs="avc1.77.30,mp4a.40.2"',
      'video/mp2t; codecs="avc1.100.40,mp4a.40.2"',
    ],
    'maybe': [
      'video/mp2t',
    ],
    'not': [
      // audio/mp2t is currently not supported (see also crbug.com/556837).
      'audio/mp2t; codecs="mp4a.40.2"',

      // H.264 + AC3/EAC3 audio Variants
      'video/mp2t; codecs="avc1.640028,ac-3"',
      'video/mp2t; codecs="avc1.640028,ec-3"',
      'video/mp2t; codecs="avc1.640028,mp4a.A5"',
      'video/mp2t; codecs="avc1.640028,mp4a.A6"',
      'video/mp2t; codecs="avc1.640028,mp4a.a5"',
      'video/mp2t; codecs="avc1.640028,mp4a.a6"',
    ],
  };

  return testBadMpegVariants('video/mp2t') &&
      testMimeCodecMap(MP2T_CODEC_MAP, !has_mp2ts_support)
}

function testNewVp9Variants(has_profile_2_3_support) {
  // Malformed codecs string never allowed.
  const VP9_BAD_CODEC_LIST = ['vp09.00.-1.08'];

  // Test a few valid strings.
  const VP9_GOOD_CODEC_LIST = [
    'vp09.00.10.08',
    'vp09.00.10.08.00.01.01.01.00',
    'vp09.00.10.08.01.02.02.02.00',
    'vp09.01.10.08',
  ];

  // Profiles 0 and 1 are always supported supported. Profiles 2 and 3 are
  // only supported on certain architectures.
  const VP9_2_3_CODEC_LIST = [
    'vp09.02.10.08',
    'vp09.03.10.08',
  ];

  let PROBABLY_23 = has_profile_2_3_support ? 'probably' : '';

  const MIME_TYPES = ['video/webm', 'video/mp4'];
  for (var i = 0; i < MIME_TYPES.length; ++i) {
    let mime = MIME_TYPES[i];
    if (!testCodecList(mime, VP9_BAD_CODEC_LIST, ''))
      return false;
    if (!testCodecList(mime, VP9_GOOD_CODEC_LIST, 'probably'))
      return false;
    if (!testCodecList(mime, VP9_2_3_CODEC_LIST, PROBABLY_23))
      return false;
  }

  return true;
}

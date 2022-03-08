'use strict';

// The OpenType spec mentions that the follow tables are required for a font to
// function correctly. We'll have all the tables listed except for OS/2, which
// is not present in all fonts on Mac OS.
// https://docs.microsoft.com/en-us/typography/opentype/spec/otff#font-tables
const BASE_TABLES = [
  'cmap',
  'head',
  'hhea',
  'hmtx',
  'maxp',
  'name',
  'post',
];

const MAC_FONTS = new Map([
  ['Monaco', {
    postscriptName: 'Monaco',
    fullName: 'Monaco',
    family: 'Monaco',
    style: 'Regular',
    expectedTables: [
      // Tables related to TrueType.
      'cvt ',
      'glyf',
      'loca',
      'prep',
      'gasp',
    ],
  }],
  ['Menlo-Regular', {
    postscriptName: 'Menlo-Regular',
    fullName: 'Menlo Regular',
    family: 'Menlo',
    style: 'Regular',
    expectedTables: [
      'cvt ',
      'glyf',
      'loca',
      'prep',
    ],
  }],
  // Indic.
  ['GujaratiMT', {
    postscriptName: 'GujaratiMT',
    fullName: 'Gujarati MT',
    family: 'Gujarati MT',
    style: 'Regular',
    expectedTables: [
      'cvt ',
      'glyf',
      'loca',
      'prep',
    ],
  }],
  // Japanese.
  ['HiraMinProN-W3', {
    postscriptName: 'HiraMinProN-W3',
    fullName: 'Hiragino Mincho ProN W3',
    family: 'Hiragino Mincho ProN',
    style: 'W3',
    expectedTables: [
      'CFF ',
      'VORG',
    ],
  }],
  // Korean.
  ['AppleGothic', {
    postscriptName: 'AppleGothic',
    fullName: 'AppleGothic Regular',
    family: 'AppleGothic',
    style: 'Regular',
    expectedTables: [
      'cvt ',
      'glyf',
      'loca',
    ],
  }],
  // Chinese.
  ['STHeitiTC-Medium', {
    postscriptName: 'STHeitiTC-Medium',
    fullName: 'Heiti TC Medium',
    family: 'Heiti TC',
    style: 'Medium',
    expectedTables: [
      'cvt ',
      'glyf',
      'loca',
      'prep',
    ],
  }],
  // Bitmap.
  ['AppleColorEmoji', {
    postscriptName: 'AppleColorEmoji',
    fullName: 'Apple Color Emoji',
    family: 'Apple Color Emoji',
    style: 'Regular',
    expectedTables: [
      'glyf',
      'loca',
      // Tables related to Bitmap Glyphs.
      'sbix',
    ],
  }],
]);

const WIN_FONTS = new Map([
  ['Verdana', {
    postscriptName: 'Verdana',
    fullName: 'Verdana',
    family: 'Verdana',
    style: 'Regular',
    expectedTables: [
      // Tables related to TrueType.
      'cvt ',
      'glyf',
      'loca',
      'prep',
      'gasp',
    ],
  }],
  // Korean.
  ['MalgunGothicBold', {
    postscriptName: 'MalgunGothicBold',
    fullName: 'Malgun Gothic Bold',
    family: 'Malgun Gothic',
    style: 'Bold',
    expectedTables: [
      // Tables related to TrueType.
      'cvt ',
      'glyf',
      'loca',
      'prep',
      'gasp',
    ],
  }],
]);

const LINUX_FONTS = new Map([
  ['Ahem', {
    postscriptName: 'Ahem',
    fullName: 'Ahem',
    family: 'Ahem',
    style: 'Regular',
    expectedTables: [
      // Tables related to TrueType.
      'cvt ',
      'glyf',
      'loca',
      'prep',
      'gasp',
    ],
  }],
]);

// Returns a map of known system fonts, mapping a font's postscript name to
// FontMetadata.
function getTestData() {
  let output = undefined;
  if (navigator.platform.indexOf("Win") !== -1) {
    output = WIN_FONTS;
  } else if (navigator.platform.indexOf("Mac") !== -1) {
    output = MAC_FONTS;
  } else if (navigator.platform.indexOf("Linux") !== -1) {
    output = LINUX_FONTS;
  }

  assert_not_equals(
      output, undefined, 'Cannot get test set due to unsupported platform.');

  return output;
}
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_FILE_EXTENSION_H_
#define PDF_FILE_EXTENSION_H_

#include <string>

namespace chrome_pdf {

// The indexes should always match `ViewFileType` in
// tools/metrics/histograms/enums.xml and should never be renumbered.
enum class ExtensionIndex : int {
  kOtherExt = 0,
  k3ga = 1,
  k3gp = 2,
  kAac = 3,
  kAlac = 4,
  kAsf = 5,
  kAvi = 6,
  kBmp = 7,
  kCsv = 8,
  kDoc = 9,
  kDocx = 10,
  kFlac = 11,
  kGif = 12,
  kJpeg = 13,
  kJpg = 14,
  kLog = 15,
  kM3u = 16,
  kM3u8 = 17,
  kM4a = 18,
  kM4v = 19,
  kMid = 20,
  kMkv = 21,
  kMov = 22,
  kMp3 = 23,
  kMp4 = 24,
  kMpg = 25,
  kOdf = 26,
  kOdp = 27,
  kOds = 28,
  kOdt = 29,
  kOga = 30,
  kOgg = 31,
  kOgv = 32,
  kPdf = 33,
  kPng = 34,
  kPpt = 35,
  kPptx = 36,
  kRa = 37,
  kRam = 38,
  kRar = 39,
  kRm = 40,
  kRtf = 41,
  kWav = 42,
  kWebm = 43,
  kWebp = 44,
  kWma = 45,
  kWmv = 46,
  kXls = 47,
  kXlsx = 48,
  kCrdownload = 49,
  kCrx = 50,
  kDmg = 51,
  kExe = 52,
  kHtml = 53,
  kHtm = 54,
  kJar = 55,
  kPs = 56,
  kTorrent = 57,
  kTxt = 58,
  kZip = 59,
  kDirectory = 60,
  kEmptyExt = 61,
  kUnknownExt = 62,
  kMhtml = 63,
  kGdoc = 64,
  kGsheet = 65,
  kGslides = 66,
  kArw = 67,
  kCr2 = 68,
  kDng = 69,
  kNef = 70,
  kNrw = 71,
  kOrf = 72,
  kRaf = 73,
  kRw2 = 74,
  kTini = 75,
  kMaxValue = kTini,
};

enum ExtensionIndex FileNameToExtensionIndex(const std::u16string& file_name);

}  // namespace chrome_pdf

#endif  // PDF_FILE_EXTENSION_H_

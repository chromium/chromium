// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MIMETYPE_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MIMETYPE_UTIL_H_

#include <string>

// Enum for the Download.IOSDownloadMimeType UMA histogram to report the
// MIME type of the download task.
// Note: This enum is used to back an UMA histogram, and should be treated as
// append-only.
// LINT.IfChange
enum class DownloadMimeTypeResult {
  // MIME type other than those listed below.
  Other = 0,
  // application/vnd.apple.pkpass MIME type.
  PkPass = 1,
  // application/x-apple-aspen-config MIME type.
  iOSMobileConfig = 2,
  // application/zip MIME type.
  ZipArchive = 3,
  // application/x-msdownload MIME type (.exe file).
  MicrosoftApplication = 4,
  // application/vnd.android.package-archive MIME type (.apk file).
  AndroidPackageArchive = 5,
  // text/vcard MIME type.
  VirtualContactFile = 6,
  // text/calendar MIME type.
  Calendar = 7,
  // model/usd MIME type.
  LegacyUniversalSceneDescription = 8,
  // application/x-apple-diskimage MIME type.
  AppleDiskImage = 9,
  // application/vnd.apple.installer+xml MIME type.
  AppleInstallerPackage = 10,
  // application/x-7z-compressed MIME type.
  SevenZipArchive = 11,
  // application/x-rar-compressed MIME type.
  RARArchive = 12,
  // application/x-tar MIME type.
  TarArchive = 13,
  // application/x-shockwave-flash MIME type.
  AdobeFlash = 14,
  // application/vnd.amazon.ebook MIME type.
  AmazonKindleBook = 15,
  // application/octet-stream MIME type.
  BinaryData = 16,
  // application/x-bittorrent MIME type.
  BitTorrent = 17,
  // application/java-archive MIME type.
  JavaArchive = 18,
  // model/vnd.pixar.usd MIME type.
  LegacyPixarUniversalSceneDescription = 19,
  // model/vnd.usdz+zip MIME type.
  UniversalSceneDescription = 20,
  // text/vcard MIME type.
  Vcard = 21,
  AACAudio = 22,
  AbiWordDocument = 23,
  ArchiveDocument = 24,
  AVIFImage = 25,
  AVIVideo = 26,
  GenericBitmap = 27,
  MicrosoftBitmap = 28,
  BZip2Archive = 29,
  CDAudio = 30,
  CShellScript = 31,
  CascadingStyleSheet = 32,
  CommaSeparatedValues = 33,
  MicrosoftWord = 34,
  MicrosoftWordXML = 35,
  MSEmbeddedOpenTypefont = 36,
  ElectronicPublication = 37,
  GZipCompressedArchive = 38,
  GraphicsInterchangeFormat = 39,
  HyperTextMarkupLanguage = 40,
  IconFormat = 41,
  JPEGImage = 42,
  JavaScript = 43,
  JSONFormat = 44,
  JSONLDFormat = 45,
  MusicalInstrumentDigitalInterface = 46,
  XMusicalInstrumentDigitalInterface = 47,
  MP3Audio = 48,
  MP4Video = 49,
  MPEGVideo = 50,
  OpenDocumentPresentationDocument = 51,
  OpenDocumentSpreadsheetDocument = 52,
  OpenDocumentTextDocument = 53,
  OGGAudio = 54,
  OGGVideo = 55,
  OGG = 56,
  OpusAudio = 57,
  OpenTypeFont = 58,
  PortableNetworkGraphic = 59,
  AdobePortableDocumentFormat = 60,
  HypertextPreprocessor = 61,
  MicrosoftPowerPoint = 62,
  MicrosoftPowerPointOpenXML = 63,
  RARArchiveVND = 64,
  RichTextFormat = 65,
  BourneShellScript = 66,
  ScalableVectorGraphic = 67,
  TaggedImageFileFormat = 68,
  MPEGTransportStream = 69,
  TrueTypeFont = 70,
  Text = 71,
  MicrosoftVisio = 72,
  WaveformAudioFormat = 73,
  WEBMAudio = 74,
  WEBMVideo = 75,
  WEBPImage = 76,
  WebOpenFont = 77,
  WebOpenFont2 = 78,
  XHTML = 79,
  MicrosoftExcel = 80,
  MicrosoftExcelOpenXML = 81,
  XML = 82,
  XUL = 83,
  k3GPPVideo = 84,
  k3GPPAudio = 85,
  k3GPP2Video = 86,
  k3GPP2Audio = 87,
  kBundledPkPass = 88,
  kMaxValue = kBundledPkPass,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

// Returns DownloadMimeTypeResult for the given MIME type.
DownloadMimeTypeResult GetDownloadMimeTypeResultFromMimeType(
    const std::string& mime_type);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_MIMETYPE_UTIL_H_

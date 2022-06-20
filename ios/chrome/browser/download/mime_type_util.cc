// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/mime_type_util.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"

// Extensions.
char kUsdzFileExtension[] = ".usdz";
char kRealityFileExtension[] = ".reality";

// MIME types.
char kMobileConfigurationType[] = "application/x-apple-aspen-config";
char kPkPassMimeType[] = "application/vnd.apple.pkpass";
char kVcardMimeType[] = "text/vcard";
char kUsdzMimeType[] = "model/vnd.usdz+zip";
char kLegacyUsdzMimeType[] = "model/usd";
char kLegacyPixarUsdzMimeType[] = "model/vnd.pixar.usd";
char kZipArchiveMimeType[] = "application/zip";
char kMicrosoftApplicationMimeType[] = "application/x-msdownload";
char kAndroidPackageArchiveMimeType[] =
    "application/vnd.android.package-archive";
char kCalendarMimeType[] = "text/calendar";
char kAppleDiskImageMimeType[] = "application/x-apple-diskimage";
char kAppleInstallerPackageMimeType[] = "application/vnd.apple.installer+xml";
char kSevenZipArchiveMimeType[] = "application/x-7z-compressed";
char kRARArchiveMimeType[] = "application/x-rar-compressed";
char kTarArchiveMimeType[] = "application/x-tar";
char kAdobeFlashMimeType[] = "application/x-shockwave-flash";
char kAmazonKindleBookMimeType[] = "application/vnd.amazon.ebook";
char kBinaryDataMimeType[] = "application/octet-stream";
char kBitTorrentMimeType[] = "application/x-bittorrent";
char kJavaArchiveMimeType[] = "application/java-archive";
char kAACAudioMimetype[] = "audio/aac";
char kAbiWordDocumentMimetype[] = "application/x-abiword";
char kArchiveDocumentMimetype[] = "application/x-freearc";
char kAVIFImageMimetype[] = "image/avif";
char kAVIVideoMimetype[] = "video/x-msvideo";
char kGenericBitmapMimetype[] = "image/bmp";
char kMicrosoftBitmapMimetype[] = "image/x-ms-bmp";
char kBZipArchiveMimetype[] = "application/x-bzip";
char kBZip2ArchiveMimetype[] = "application/x-bzip2";
char kCDAudioMimetype[] = "application/x-cdf";
char kCShellScriptMimetype[] = "application/x-csh";
char kCascadingStyleSheetMimetype[] = "text/css";
char kCommaSeparatedValuesMimetype[] = "text/csv";
char kMicrosoftWordMimetype[] = "application/msword";
char kMicrosoftWordXMLMimetype[] =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
char kMSEmbeddedOpenTypefontMimetype[] = "application/vnd.ms-fontobject";
char kElectronicPublicationMimetype[] = "application/epub+zip";
char kGZipCompressedArchiveMimetype[] = "application/gzip";
char kGraphicsInterchangeFormatMimetype[] = "image/gif";
char kHyperTextMarkupLanguageMimetype[] = "text/html";
char kIconFormatMimetype[] = "image/vnd.microsoft.icon";
char kJPEGImageMimetype[] = "image/jpeg";
char kJavaScriptMimetype[] = "text/javascript";
char kJSONFormatMimetype[] = "application/json";
char kJSONLDFormatMimetype[] = "application/ld+json";
char kMusicalInstrumentDigitalInterfaceMimetype[] = "audio/midi";
char kXMusicalInstrumentDigitalInterfaceMimetype[] = "audio/x-midi";
char kMP3AudioMimetype[] = "audio/mpeg";
char kMP4VideoMimetype[] = "video/mp4";
char kMPEGVideoMimetype[] = "video/mpeg";
char kOpenDocumentPresentationDocumentMimetype[] =
    "application/vnd.oasis.opendocument.presentation";
char kOpenDocumentSpreadsheetDocumentMimetype[] =
    "application/vnd.oasis.opendocument.spreadsheet";
char kOpenDocumentTextDocumentMimetype[] =
    "application/vnd.oasis.opendocument.text";
char kOGGAudioMimetype[] = "audio/ogg";
char kOGGVideoMimetype[] = "video/ogg";
char kOGGMimetype[] = "application/ogg";
char kOpusAudioMimetype[] = "audio/opus";
char kOpenTypeFontMimetype[] = "font/otf";
char kPortableNetworkGraphicMimetype[] = "image/png";
char kAdobePortableDocumentFormatMimetype[] = "application/pdf";
char kHypertextPreprocessorMimetype[] = "application/x-httpd-php";
char kMicrosoftPowerPointMimetype[] = "application/vnd.ms-powerpoint";
char kMicrosoftPowerPointOpenXMLMimetype[] =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";
char kRARArchiveMimetype[] = "application/vnd.rar";
char kRichTextFormatMimetype[] = "application/rtf";
char kBourneShellScriptMimetype[] = "application/x-sh";
char kScalableVectorGraphicMimetype[] = "image/svg+xml";
char kTaggedImageFileFormatMimetype[] = "image/tiff";
char kMPEGTransportStreamMimetype[] = "video/mp2t";
char kTrueTypeFontMimetype[] = "font/ttf";
char kTextMimetype[] = "text/plain";
char kMicrosoftVisioMimetype[] = "application/vnd.visio";
char kWaveformAudioFormatMimetype[] = "audio/wav";
char kWEBMAudioMimetype[] = "audio/webm";
char kWEBMVideoMimetype[] = "video/webm";
char kWEBPImageMimetype[] = "image/webp";
char kWebOpenFontMimetype[] = "font/woff";
char kWebOpenFont2Mimetype[] = "font/woff2";
char kXHTMLMimetype[] = "application/xhtml+xml";
char kMicrosoftExcelMimetype[] = "application/vnd.ms-excel";
char kMicrosoftExcelOpenXMLMimetype[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
char kXMLMimetype[] = "application/xml";
char kXULMimetype[] = "application/vnd.mozilla.xul+xml";
char k3GPPVideoMimetype[] = "video/3gpp";
char k3GPPAudioMimetype[] = "audio/3gpp";
char k3GPP2VideoMimetype[] = "video/3gpp2";
char k3GPP2AudioMimetype[] = "audio/3gpp2";

bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::FilePath& suggested_path) {
  return mime_type == kUsdzMimeType || mime_type == kLegacyUsdzMimeType ||
         mime_type == kLegacyPixarUsdzMimeType ||
         suggested_path.MatchesExtension(kUsdzFileExtension) ||
         suggested_path.MatchesExtension(kRealityFileExtension);
}

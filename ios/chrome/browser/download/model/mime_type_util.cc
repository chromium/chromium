// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/mime_type_util.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"

// Extensions.
char kUsdzFileExtension[] = ".usdz";
char kRealityFileExtension[] = ".reality";

// MIME types.
char kMobileConfigurationType[] = "application/x-apple-aspen-config";
char kPkPassMimeType[] = "application/vnd.apple.pkpass";
char kPkBundledPassMimeType[] = "application/vnd.apple.pkpasses";
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
char kAACAudioMimeType[] = "audio/aac";
char kAbiWordDocumentMimeType[] = "application/x-abiword";
char kArchiveDocumentMimeType[] = "application/x-freearc";
char kAVIFImageMimeType[] = "image/avif";
char kAVIVideoMimeType[] = "video/x-msvideo";
char kGenericBitmapMimeType[] = "image/bmp";
char kMicrosoftBitmapMimeType[] = "image/x-ms-bmp";
char kBZipArchiveMimeType[] = "application/x-bzip";
char kBZip2ArchiveMimeType[] = "application/x-bzip2";
char kCDAudioMimeType[] = "application/x-cdf";
char kCShellScriptMimeType[] = "application/x-csh";
char kCascadingStyleSheetMimeType[] = "text/css";
char kCommaSeparatedValuesMimeType[] = "text/csv";
char kMicrosoftWordMimeType[] = "application/msword";
char kMicrosoftWordXMLMimeType[] =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
char kMSEmbeddedOpenTypefontMimeType[] = "application/vnd.ms-fontobject";
char kElectronicPublicationMimeType[] = "application/epub+zip";
char kGZipCompressedArchiveMimeType[] = "application/gzip";
char kGraphicsInterchangeFormatMimeType[] = "image/gif";
char kHyperTextMarkupLanguageMimeType[] = "text/html";
char kIconFormatMimeType[] = "image/vnd.microsoft.icon";
char kJPEGImageMimeType[] = "image/jpeg";
char kJavaScriptMimeType[] = "text/javascript";
char kJSONFormatMimeType[] = "application/json";
char kJSONLDFormatMimeType[] = "application/ld+json";
char kMusicalInstrumentDigitalInterfaceMimeType[] = "audio/midi";
char kXMusicalInstrumentDigitalInterfaceMimeType[] = "audio/x-midi";
char kMP3AudioMimeType[] = "audio/mpeg";
char kMP4VideoMimeType[] = "video/mp4";
char kMPEGVideoMimeType[] = "video/mpeg";
char kOpenDocumentPresentationDocumentMimeType[] =
    "application/vnd.oasis.opendocument.presentation";
char kOpenDocumentSpreadsheetDocumentMimeType[] =
    "application/vnd.oasis.opendocument.spreadsheet";
char kOpenDocumentTextDocumentMimeType[] =
    "application/vnd.oasis.opendocument.text";
char kOGGAudioMimeType[] = "audio/ogg";
char kOGGVideoMimeType[] = "video/ogg";
char kOGGMimeType[] = "application/ogg";
char kOpusAudioMimeType[] = "audio/opus";
char kOpenTypeFontMimeType[] = "font/otf";
char kPortableNetworkGraphicMimeType[] = "image/png";
char kAdobePortableDocumentFormatMimeType[] = "application/pdf";
char kHypertextPreprocessorMimeType[] = "application/x-httpd-php";
char kMicrosoftPowerPointMimeType[] = "application/vnd.ms-powerpoint";
char kMicrosoftPowerPointOpenXMLMimeType[] =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";
char kRARArchiveVNDMimeType[] = "application/vnd.rar";
char kRichTextFormatMimeType[] = "application/rtf";
char kBourneShellScriptMimeType[] = "application/x-sh";
char kScalableVectorGraphicMimeType[] = "image/svg+xml";
char kTaggedImageFileFormatMimeType[] = "image/tiff";
char kMPEGTransportStreamMimeType[] = "video/mp2t";
char kTrueTypeFontMimeType[] = "font/ttf";
char kTextMimeType[] = "text/plain";
char kMicrosoftVisioMimeType[] = "application/vnd.visio";
char kWaveformAudioFormatMimeType[] = "audio/wav";
char kWEBMAudioMimeType[] = "audio/webm";
char kWEBMVideoMimeType[] = "video/webm";
char kWEBPImageMimeType[] = "image/webp";
char kWebOpenFontMimeType[] = "font/woff";
char kWebOpenFont2MimeType[] = "font/woff2";
char kXHTMLMimeType[] = "application/xhtml+xml";
char kMicrosoftExcelMimeType[] = "application/vnd.ms-excel";
char kMicrosoftExcelOpenXMLMimeType[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
char kXMLMimeType[] = "application/xml";
char kXULMimeType[] = "application/vnd.mozilla.xul+xml";
char k3GPPVideoMimeType[] = "video/3gpp";
char k3GPPAudioMimeType[] = "audio/3gpp";
char k3GPP2VideoMimeType[] = "video/3gpp2";
char k3GPP2AudioMimeType[] = "audio/3gpp2";
char kAnimatedPortableNetworkGraphicsImageMimeType[] = "image/apng";

bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::FilePath& suggested_path) {
  return mime_type == kUsdzMimeType || mime_type == kLegacyUsdzMimeType ||
         mime_type == kLegacyPixarUsdzMimeType ||
         suggested_path.MatchesExtension(kUsdzFileExtension) ||
         suggested_path.MatchesExtension(kRealityFileExtension);
}

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_MIME_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_MIME_TYPE_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

// MIME type for iOS configuration file.
extern char kMobileConfigurationType[];

// MIME type for Virtual Contact File.
extern char kVcardMimeType[];

// MIME type for pass data.
extern char kPkPassMimeType[];

// MIME type for bundled pass data.
extern char kPkBundledPassMimeType[];

// MIME Type for 3D models.
extern char kUsdzMimeType[];

// MIME Type for ZIP archive.
extern char kZipArchiveMimeType[];

// MIME Type for Microsoft application.
extern char kMicrosoftApplicationMimeType[];

// MIME Type for Android package archive.
extern char kAndroidPackageArchiveMimeType[];

// MIME Type for Calendar.
extern char kCalendarMimeType[];

// MIME Type for Apple disk image.
extern char kAppleDiskImageMimeType[];

// MIME Type for Apple installer package.
extern char kAppleInstallerPackageMimeType[];

// MIME Type for 7z archive.
extern char kSevenZipArchiveMimeType[];

// MIME Type for RAR archive.
extern char kRARArchiveMimeType[];

// MIME Type for TAR archive.
extern char kTarArchiveMimeType[];

// MIME Type for Adobe Flash.
extern char kAdobeFlashMimeType[];

// MIME Type for Amazon kindle book.
extern char kAmazonKindleBookMimeType[];

// MIME Type for binary data.
extern char kBinaryDataMimeType[];

// MIME Type for BitTorrent.
extern char kBitTorrentMimeType[];

// MIME Type for Java archive.
extern char kJavaArchiveMimeType[];

// Legacy USDZ content types.
extern char kLegacyUsdzMimeType[];
extern char kLegacyPixarUsdzMimeType[];

// MIME Types that don't have specific treatment.
extern char kAACAudioMimeType[];
extern char kAbiWordDocumentMimeType[];
extern char kArchiveDocumentMimeType[];
extern char kAVIFImageMimeType[];
extern char kAVIVideoMimeType[];
extern char kGenericBitmapMimeType[];
extern char kMicrosoftBitmapMimeType[];
extern char kBZip2ArchiveMimeType[];
extern char kCDAudioMimeType[];
extern char kCShellScriptMimeType[];
extern char kCascadingStyleSheetMimeType[];
extern char kCommaSeparatedValuesMimeType[];
extern char kMicrosoftWordMimeType[];
extern char kMicrosoftWordXMLMimeType[];
extern char kMSEmbeddedOpenTypefontMimeType[];
extern char kElectronicPublicationMimeType[];
extern char kGZipCompressedArchiveMimeType[];
extern char kGraphicsInterchangeFormatMimeType[];
extern char kHyperTextMarkupLanguageMimeType[];
extern char kIconFormatMimeType[];
extern char kJPEGImageMimeType[];
extern char kJavaScriptMimeType[];
extern char kJSONFormatMimeType[];
extern char kJSONLDFormatMimeType[];
extern char kMusicalInstrumentDigitalInterfaceMimeType[];
extern char kXMusicalInstrumentDigitalInterfaceMimeType[];
extern char kMP3AudioMimeType[];
extern char kMP4VideoMimeType[];
extern char kMPEGVideoMimeType[];
extern char kOpenDocumentPresentationDocumentMimeType[];
extern char kOpenDocumentSpreadsheetDocumentMimeType[];
extern char kOpenDocumentTextDocumentMimeType[];
extern char kOGGAudioMimeType[];
extern char kOGGVideoMimeType[];
extern char kOGGMimeType[];
extern char kOpusAudioMimeType[];
extern char kOpenTypeFontMimeType[];
extern char kPortableNetworkGraphicMimeType[];
extern char kAdobePortableDocumentFormatMimeType[];
extern char kHypertextPreprocessorMimeType[];
extern char kMicrosoftPowerPointMimeType[];
extern char kMicrosoftPowerPointOpenXMLMimeType[];
extern char kRARArchiveVNDMimeType[];
extern char kRichTextFormatMimeType[];
extern char kBourneShellScriptMimeType[];
extern char kScalableVectorGraphicMimeType[];
extern char kTaggedImageFileFormatMimeType[];
extern char kMPEGTransportStreamMimeType[];
extern char kTrueTypeFontMimeType[];
extern char kTextMimeType[];
extern char kMicrosoftVisioMimeType[];
extern char kWaveformAudioFormatMimeType[];
extern char kWEBMAudioMimeType[];
extern char kWEBMVideoMimeType[];
extern char kWEBPImageMimeType[];
extern char kWebOpenFontMimeType[];
extern char kWebOpenFont2MimeType[];
extern char kXHTMLMimeType[];
extern char kMicrosoftExcelMimeType[];
extern char kMicrosoftExcelOpenXMLMimeType[];
extern char kXMLMimeType[];
extern char kXULMimeType[];
extern char k3GPPVideoMimeType[];
extern char k3GPPAudioMimeType[];
extern char k3GPP2VideoMimeType[];
extern char k3GPP2AudioMimeType[];
extern char kAnimatedPortableNetworkGraphicsImageMimeType[];

// Returns whether the content-type or the file extension match those of a USDZ
// 3D model. The file extension is checked in addition to the content-type since
// many static file hosting services do not allow setting the content-type.
bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::FilePath& suggested_filename);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_MIME_TYPE_UTIL_H_

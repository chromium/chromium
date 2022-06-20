// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_

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
extern char kAACAudioMimetype[];
extern char kAbiWordDocumentMimetype[];
extern char kArchiveDocumentMimetype[];
extern char kAVIFImageMimetype[];
extern char kAVIVideoMimetype[];
extern char kGenericBitmapMimetype[];
extern char kMicrosoftBitmapMimetype[];
extern char kBZip2ArchiveMimetype[];
extern char kCDAudioMimetype[];
extern char kCShellScriptMimetype[];
extern char kCascadingStyleSheetMimetype[];
extern char kCommaSeparatedValuesMimetype[];
extern char kMicrosoftWordMimetype[];
extern char kMicrosoftWordXMLMimetype[];
extern char kMSEmbeddedOpenTypefontMimetype[];
extern char kElectronicPublicationMimetype[];
extern char kGZipCompressedArchiveMimetype[];
extern char kGraphicsInterchangeFormatMimetype[];
extern char kHyperTextMarkupLanguageMimetype[];
extern char kIconFormatMimetype[];
extern char kJPEGImageMimetype[];
extern char kJavaScriptMimetype[];
extern char kJSONFormatMimetype[];
extern char kJSONLDFormatMimetype[];
extern char kMusicalInstrumentDigitalInterfaceMimetype[];
extern char kXMusicalInstrumentDigitalInterfaceMimetype[];
extern char kMP3AudioMimetype[];
extern char kMP4VideoMimetype[];
extern char kMPEGVideoMimetype[];
extern char kOpenDocumentPresentationDocumentMimetype[];
extern char kOpenDocumentSpreadsheetDocumentMimetype[];
extern char kOpenDocumentTextDocumentMimetype[];
extern char kOGGAudioMimetype[];
extern char kOGGVideoMimetype[];
extern char kOGGMimetype[];
extern char kOpusAudioMimetype[];
extern char kOpenTypeFontMimetype[];
extern char kPortableNetworkGraphicMimetype[];
extern char kAdobePortableDocumentFormatMimetype[];
extern char kHypertextPreprocessorMimetype[];
extern char kMicrosoftPowerPointMimetype[];
extern char kMicrosoftPowerPointOpenXMLMimetype[];
extern char kRARArchiveMimetype[];
extern char kRichTextFormatMimetype[];
extern char kBourneShellScriptMimetype[];
extern char kScalableVectorGraphicMimetype[];
extern char kTaggedImageFileFormatMimetype[];
extern char kMPEGTransportStreamMimetype[];
extern char kTrueTypeFontMimetype[];
extern char kTextMimetype[];
extern char kMicrosoftVisioMimetype[];
extern char kWaveformAudioFormatMimetype[];
extern char kWEBMAudioMimetype[];
extern char kWEBMVideoMimetype[];
extern char kWEBPImageMimetype[];
extern char kWebOpenFontMimetype[];
extern char kWebOpenFont2Mimetype[];
extern char kXHTMLMimetype[];
extern char kMicrosoftExcelMimetype[];
extern char kMicrosoftExcelOpenXMLMimetype[];
extern char kXMLMimetype[];
extern char kXULMimetype[];
extern char k3GPPVideoMimetype[];
extern char k3GPPAudioMimetype[];
extern char k3GPP2VideoMimetype[];
extern char k3GPP2AudioMimetype[];

// Returns whether the content-type or the file extension match those of a USDZ
// 3D model. The file extension is checked in addition to the content-type since
// many static file hosting services do not allow setting the content-type.
bool IsUsdzFileFormat(const std::string& mime_type,
                      const base::FilePath& suggested_filename);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MIME_TYPE_UTIL_H_

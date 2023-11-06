// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/download/model/download_mimetype_util.h"
#include "ios/chrome/browser/download/model/mime_type_util.h"

DownloadMimeTypeResult GetDownloadMimeTypeResultFromMimeType(
    const std::string& mime_type) {
  if (mime_type == kPkPassMimeType)
    return DownloadMimeTypeResult::PkPass;

  if (mime_type == kPkBundledPassMimeType) {
    return DownloadMimeTypeResult::kBundledPkPass;
  }

  if (mime_type == kZipArchiveMimeType)
    return DownloadMimeTypeResult::ZipArchive;

  if (mime_type == kMobileConfigurationType)
    return DownloadMimeTypeResult::iOSMobileConfig;

  if (mime_type == kMicrosoftApplicationMimeType)
    return DownloadMimeTypeResult::MicrosoftApplication;

  if (mime_type == kAndroidPackageArchiveMimeType)
    return DownloadMimeTypeResult::AndroidPackageArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::VirtualContactFile;

  if (mime_type == kCalendarMimeType)
    return DownloadMimeTypeResult::Calendar;

  if (mime_type == kLegacyUsdzMimeType)
    return DownloadMimeTypeResult::LegacyUniversalSceneDescription;

  if (mime_type == kAppleDiskImageMimeType)
    return DownloadMimeTypeResult::AppleDiskImage;

  if (mime_type == kAppleInstallerPackageMimeType)
    return DownloadMimeTypeResult::AppleInstallerPackage;

  if (mime_type == kSevenZipArchiveMimeType)
    return DownloadMimeTypeResult::SevenZipArchive;

  if (mime_type == kRARArchiveMimeType)
    return DownloadMimeTypeResult::RARArchive;

  if (mime_type == kTarArchiveMimeType)
    return DownloadMimeTypeResult::TarArchive;

  if (mime_type == kAdobeFlashMimeType)
    return DownloadMimeTypeResult::AdobeFlash;

  if (mime_type == kAmazonKindleBookMimeType)
    return DownloadMimeTypeResult::AmazonKindleBook;

  if (mime_type == kBinaryDataMimeType)
    return DownloadMimeTypeResult::BinaryData;

  if (mime_type == kBitTorrentMimeType)
    return DownloadMimeTypeResult::BitTorrent;

  if (mime_type == kJavaArchiveMimeType)
    return DownloadMimeTypeResult::JavaArchive;

  if (mime_type == kVcardMimeType)
    return DownloadMimeTypeResult::Vcard;

  if (mime_type == kLegacyPixarUsdzMimeType)
    return DownloadMimeTypeResult::LegacyPixarUniversalSceneDescription;

  if (mime_type == kUsdzMimeType)
    return DownloadMimeTypeResult::UniversalSceneDescription;

  if (mime_type == kAACAudioMimeType)
    return DownloadMimeTypeResult::AACAudio;

  if (mime_type == kAbiWordDocumentMimeType)
    return DownloadMimeTypeResult::AbiWordDocument;

  if (mime_type == kArchiveDocumentMimeType)
    return DownloadMimeTypeResult::ArchiveDocument;

  if (mime_type == kAVIFImageMimeType)
    return DownloadMimeTypeResult::AVIFImage;

  if (mime_type == kAVIVideoMimeType)
    return DownloadMimeTypeResult::AVIVideo;

  if (mime_type == kGenericBitmapMimeType)
    return DownloadMimeTypeResult::GenericBitmap;

  if (mime_type == kMicrosoftBitmapMimeType)
    return DownloadMimeTypeResult::MicrosoftBitmap;

  if (mime_type == kBZip2ArchiveMimeType)
    return DownloadMimeTypeResult::BZip2Archive;

  if (mime_type == kCDAudioMimeType)
    return DownloadMimeTypeResult::CDAudio;

  if (mime_type == kCShellScriptMimeType)
    return DownloadMimeTypeResult::CShellScript;

  if (mime_type == kCascadingStyleSheetMimeType)
    return DownloadMimeTypeResult::CascadingStyleSheet;

  if (mime_type == kCommaSeparatedValuesMimeType)
    return DownloadMimeTypeResult::CommaSeparatedValues;

  if (mime_type == kMicrosoftWordMimeType)
    return DownloadMimeTypeResult::MicrosoftWord;

  if (mime_type == kMicrosoftWordXMLMimeType)
    return DownloadMimeTypeResult::MicrosoftWordXML;

  if (mime_type == kMSEmbeddedOpenTypefontMimeType)
    return DownloadMimeTypeResult::MSEmbeddedOpenTypefont;

  if (mime_type == kElectronicPublicationMimeType)
    return DownloadMimeTypeResult::ElectronicPublication;

  if (mime_type == kGZipCompressedArchiveMimeType)
    return DownloadMimeTypeResult::GZipCompressedArchive;

  if (mime_type == kGraphicsInterchangeFormatMimeType)
    return DownloadMimeTypeResult::GraphicsInterchangeFormat;

  if (mime_type == kHyperTextMarkupLanguageMimeType)
    return DownloadMimeTypeResult::HyperTextMarkupLanguage;

  if (mime_type == kIconFormatMimeType)
    return DownloadMimeTypeResult::IconFormat;

  if (mime_type == kJPEGImageMimeType)
    return DownloadMimeTypeResult::JPEGImage;
  if (mime_type == kJavaScriptMimeType)
    return DownloadMimeTypeResult::JavaScript;

  if (mime_type == kJSONFormatMimeType)
    return DownloadMimeTypeResult::JSONFormat;

  if (mime_type == kJSONLDFormatMimeType)
    return DownloadMimeTypeResult::JSONLDFormat;

  if (mime_type == kMusicalInstrumentDigitalInterfaceMimeType)
    return DownloadMimeTypeResult::MusicalInstrumentDigitalInterface;

  if (mime_type == kXMusicalInstrumentDigitalInterfaceMimeType)
    return DownloadMimeTypeResult::XMusicalInstrumentDigitalInterface;

  if (mime_type == kMP3AudioMimeType)
    return DownloadMimeTypeResult::MP3Audio;

  if (mime_type == kMP4VideoMimeType)
    return DownloadMimeTypeResult::MP4Video;

  if (mime_type == kMPEGVideoMimeType)
    return DownloadMimeTypeResult::MPEGVideo;

  if (mime_type == kOpenDocumentPresentationDocumentMimeType)
    return DownloadMimeTypeResult::OpenDocumentPresentationDocument;

  if (mime_type == kOpenDocumentSpreadsheetDocumentMimeType)
    return DownloadMimeTypeResult::OpenDocumentSpreadsheetDocument;

  if (mime_type == kOpenDocumentTextDocumentMimeType)
    return DownloadMimeTypeResult::OpenDocumentTextDocument;

  if (mime_type == kOGGAudioMimeType)
    return DownloadMimeTypeResult::OGGAudio;

  if (mime_type == kOGGVideoMimeType)
    return DownloadMimeTypeResult::OGGVideo;

  if (mime_type == kOGGMimeType)
    return DownloadMimeTypeResult::OGG;

  if (mime_type == kOpusAudioMimeType)
    return DownloadMimeTypeResult::OpusAudio;

  if (mime_type == kOpenTypeFontMimeType)
    return DownloadMimeTypeResult::OpenTypeFont;

  if (mime_type == kPortableNetworkGraphicMimeType)
    return DownloadMimeTypeResult::PortableNetworkGraphic;

  if (mime_type == kAdobePortableDocumentFormatMimeType)
    return DownloadMimeTypeResult::AdobePortableDocumentFormat;

  if (mime_type == kHypertextPreprocessorMimeType)
    return DownloadMimeTypeResult::HypertextPreprocessor;

  if (mime_type == kMicrosoftPowerPointMimeType)
    return DownloadMimeTypeResult::MicrosoftPowerPoint;

  if (mime_type == kMicrosoftPowerPointOpenXMLMimeType)
    return DownloadMimeTypeResult::MicrosoftPowerPointOpenXML;

  if (mime_type == kRARArchiveVNDMimeType)
    return DownloadMimeTypeResult::RARArchiveVND;

  if (mime_type == kRichTextFormatMimeType)
    return DownloadMimeTypeResult::RichTextFormat;

  if (mime_type == kBourneShellScriptMimeType)
    return DownloadMimeTypeResult::BourneShellScript;

  if (mime_type == kScalableVectorGraphicMimeType)
    return DownloadMimeTypeResult::ScalableVectorGraphic;

  if (mime_type == kTaggedImageFileFormatMimeType)
    return DownloadMimeTypeResult::TaggedImageFileFormat;

  if (mime_type == kMPEGTransportStreamMimeType)
    return DownloadMimeTypeResult::MPEGTransportStream;

  if (mime_type == kTrueTypeFontMimeType)
    return DownloadMimeTypeResult::TrueTypeFont;

  if (mime_type == kTextMimeType)
    return DownloadMimeTypeResult::Text;

  if (mime_type == kMicrosoftVisioMimeType)
    return DownloadMimeTypeResult::MicrosoftVisio;

  if (mime_type == kWaveformAudioFormatMimeType)
    return DownloadMimeTypeResult::WaveformAudioFormat;

  if (mime_type == kWEBMAudioMimeType)
    return DownloadMimeTypeResult::WEBMAudio;

  if (mime_type == kWEBMVideoMimeType)
    return DownloadMimeTypeResult::WEBMVideo;

  if (mime_type == kWEBPImageMimeType)
    return DownloadMimeTypeResult::WEBPImage;

  if (mime_type == kWebOpenFontMimeType)
    return DownloadMimeTypeResult::WebOpenFont;

  if (mime_type == kWebOpenFont2MimeType)
    return DownloadMimeTypeResult::WebOpenFont2;

  if (mime_type == kXHTMLMimeType)
    return DownloadMimeTypeResult::XHTML;

  if (mime_type == kMicrosoftExcelMimeType)
    return DownloadMimeTypeResult::MicrosoftExcel;

  if (mime_type == kMicrosoftExcelOpenXMLMimeType)
    return DownloadMimeTypeResult::MicrosoftExcelOpenXML;

  if (mime_type == kXMLMimeType)
    return DownloadMimeTypeResult::XML;

  if (mime_type == kXULMimeType)
    return DownloadMimeTypeResult::XUL;

  if (mime_type == k3GPPVideoMimeType)
    return DownloadMimeTypeResult::k3GPPVideo;

  if (mime_type == k3GPPAudioMimeType)
    return DownloadMimeTypeResult::k3GPPAudio;

  if (mime_type == k3GPP2VideoMimeType)
    return DownloadMimeTypeResult::k3GPP2Video;

  if (mime_type == k3GPP2AudioMimeType)
    return DownloadMimeTypeResult::k3GPPAudio;

  return DownloadMimeTypeResult::Other;
}

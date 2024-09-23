// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "media/base/mime_util.h"
#include "media/filters/stream_parser_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/mime/mime_registry.mojom-blink.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

struct MimeRegistryPtrHolder {
 public:
  MimeRegistryPtrHolder() {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        mime_registry.BindNewPipeAndPassReceiver());
  }
  ~MimeRegistryPtrHolder() = default;

  mojo::Remote<mojom::blink::MimeRegistry> mime_registry;
};

std::string ToASCIIOrEmpty(const WebString& string) {
  return string.ContainsOnlyASCII() ? string.Ascii() : std::string();
}

template <typename CHARTYPE, typename SIZETYPE>
std::string ToLowerASCIIInternal(CHARTYPE* str, SIZETYPE length) {
  std::string lower_ascii;
  lower_ascii.reserve(length);
  for (CHARTYPE* p = str; p < str + length; p++)
    lower_ascii.push_back(base::ToLowerASCII(static_cast<char>(*p)));
  return lower_ascii;
}

// Does the same as ToASCIIOrEmpty, but also makes the chars lower.
std::string ToLowerASCIIOrEmpty(const String& str) {
  if (str.empty() || !str.ContainsOnlyASCIIOrEmpty())
    return std::string();
  if (str.Is8Bit())
    return ToLowerASCIIInternal(str.Characters8(), str.length());
  return ToLowerASCIIInternal(str.Characters16(), str.length());
}

STATIC_ASSERT_ENUM(MIMETypeRegistry::kNotSupported,
                   media::SupportsType::kNotSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::kSupported,
                   media::SupportsType::kSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::kMaybeSupported,
                   media::SupportsType::kMaybeSupported);

}  // namespace

String MIMETypeRegistry::GetMIMETypeForExtension(const String& ext) {
  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DEFINE_STATIC_LOCAL(MimeRegistryPtrHolder, registry_holder, ());
  String mime_type;
  if (!registry_holder.mime_registry->GetMimeTypeFromExtension(
          ext.IsNull() ? "" : ext, &mime_type)) {
    return String();
  }
  return mime_type;
}

String MIMETypeRegistry::GetWellKnownMIMETypeForExtension(const String& ext) {
  // This method must be thread safe and should not consult the OS/registry.
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(WebStringToFilePath(ext).value(),
                                         &mime_type);
  return String::FromUTF8(mime_type);
}

bool MIMETypeRegistry::IsSupportedMIMEType(const String& mime_type) {
  return blink::IsSupportedMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedImageMIMEType(const String& mime_type) {
  return blink::IsSupportedImageMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedImageResourceMIMEType(
    const String& mime_type) {
  return IsSupportedImageMIMEType(mime_type);
}

bool MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
    const String& mime_type) {
  std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  return (blink::IsSupportedImageMimeType(ascii_mime_type) ||
          (base::StartsWith(ascii_mime_type, "image/",
                            base::CompareCase::SENSITIVE) &&
           blink::IsSupportedNonImageMimeType(ascii_mime_type)));
}

bool MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(
    const String& mime_type) {
  return (EqualIgnoringASCIICase(mime_type, "image/jpeg") ||
          EqualIgnoringASCIICase(mime_type, "image/png") ||
          EqualIgnoringASCIICase(mime_type, "image/webp"));
}

bool MIMETypeRegistry::IsSupportedJavaScriptMIMEType(const String& mime_type) {
  return blink::IsSupportedJavascriptMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsJSONMimeType(const String& mime_type) {
  return blink::IsJSONMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedNonImageMIMEType(const String& mime_type) {
  return blink::IsSupportedNonImageMimeType(ToLowerASCIIOrEmpty(mime_type));
}

bool MIMETypeRegistry::IsSupportedMediaMIMEType(const String& mime_type,
                                                const String& codecs) {
  return SupportsMediaMIMEType(mime_type, codecs) != kNotSupported;
}

MIMETypeRegistry::SupportsType MIMETypeRegistry::SupportsMediaMIMEType(
    const String& mime_type,
    const String& codecs) {
  const std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  std::vector<std::string> codec_vector;
  media::SplitCodecs(ToASCIIOrEmpty(codecs), &codec_vector);
  return static_cast<SupportsType>(
      media::IsSupportedMediaFormat(ascii_mime_type, codec_vector));
}

MIMETypeRegistry::SupportsType MIMETypeRegistry::SupportsMediaSourceMIMEType(
    const String& mime_type,
    const String& codecs) {
  const std::string ascii_mime_type = ToLowerASCIIOrEmpty(mime_type);
  if (ascii_mime_type.empty())
    return kNotSupported;
  std::vector<std::string> parsed_codec_ids;
  media::SplitCodecs(ToASCIIOrEmpty(codecs), &parsed_codec_ids);
  return static_cast<SupportsType>(media::StreamParserFactory::IsTypeSupported(
      ascii_mime_type, parsed_codec_ids));
}

bool MIMETypeRegistry::IsJavaAppletMIMEType(const String& mime_type) {
  // Since this set is very limited and is likely to remain so we won't bother
  // with the overhead of using a hash set.  Any of the MIME types below may be
  // followed by any number of specific versions of the JVM, which is why we use
  // startsWith()
  return mime_type.StartsWithIgnoringASCIICase("application/x-java-applet") ||
         mime_type.StartsWithIgnoringASCIICase("application/x-java-bean") ||
         mime_type.StartsWithIgnoringASCIICase("application/x-java-vm");
}

bool MIMETypeRegistry::IsSupportedStyleSheetMIMEType(const String& mime_type) {
  return EqualIgnoringASCIICase(mime_type, "text/css");
}

bool MIMETypeRegistry::IsSupportedFontMIMEType(const String& mime_type) {
  static const unsigned kFontLen = 5;
  if (!mime_type.StartsWithIgnoringASCIICase("font/"))
    return false;
  String sub_type = mime_type.Substring(kFontLen).LowerASCII();
  return sub_type == "woff" || sub_type == "woff2" || sub_type == "otf" ||
         sub_type == "ttf" || sub_type == "sfnt";
}

bool MIMETypeRegistry::IsSupportedTextTrackMIMEType(const String& mime_type) {
  return EqualIgnoringASCIICase(mime_type, "text/vtt");
}

bool MIMETypeRegistry::IsXMLMIMEType(const String& mime_type) {
  if (EqualIgnoringASCIICase(mime_type, "text/xml") ||
      EqualIgnoringASCIICase(mime_type, "application/xml")) {
    return true;
  }

  // Per RFCs 3023 and 2045, an XML MIME type is of the form:
  // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$

  int length = mime_type.length();
  if (length < 7)
    return false;

  if (mime_type[0] == '/' || mime_type[length - 5] == '/' ||
      !mime_type.EndsWithIgnoringASCIICase("+xml"))
    return false;

  bool has_slash = false;
  for (int i = 0; i < length - 4; ++i) {
    UChar ch = mime_type[i];
    if (ch >= '0' && ch <= '9')
      continue;
    if (ch >= 'a' && ch <= 'z')
      continue;
    if (ch >= 'A' && ch <= 'Z')
      continue;
    switch (ch) {
      case '_':
      case '-':
      case '+':
      case '~':
      case '!':
      case '$':
      case '^':
      case '{':
      case '}':
      case '|':
      case '.':
      case '%':
      case '\'':
      case '`':
      case '#':
      case '&':
      case '*':
        continue;
      case '/':
        if (has_slash)
          return false;
        has_slash = true;
        continue;
      default:
        return false;
    }
  }

  return true;
}

bool MIMETypeRegistry::IsXMLExternalEntityMIMEType(const String& mime_type) {
  return EqualIgnoringASCIICase(mime_type,
                                "application/xml-external-parsed-entity") ||
         EqualIgnoringASCIICase(mime_type, "text/xml-external-parsed-entity");
}

bool MIMETypeRegistry::IsPlainTextMIMEType(const String& mime_type) {
  return mime_type.StartsWithIgnoringASCIICase("text/") &&
         !(EqualIgnoringASCIICase(mime_type, "text/html") ||
           EqualIgnoringASCIICase(mime_type, "text/xml") ||
           EqualIgnoringASCIICase(mime_type, "text/xsl"));
}

}  // namespace blink

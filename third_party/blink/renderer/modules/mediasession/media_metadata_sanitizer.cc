// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediasession/media_metadata_sanitizer.h"

#include "third_party/blink/public/mojom/mediasession/media_session.mojom-blink.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/mediasession/media_image.h"
#include "third_party/blink/renderer/modules/mediasession/media_metadata.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "url/url_constants.h"

namespace blink {

namespace {

// Constants used by the sanitizer, must be consistent with
// content::MediaMetdataSanitizer.

// Maximum length of all strings inside MediaMetadata when it is sent over mojo.
const size_t kMaxStringLength = 4 * 1024;

// Maximum type length of MediaImage, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxImageTypeLength = 2 * 127 + 1;

// Maximum number of MediaImages inside the MediaMetadata.
const size_t kMaxNumberOfMediaImages = 10;

// Maximum of sizes in a MediaImage.
const size_t kMaxNumberOfImageSizes = 10;

bool CheckMediaImageSrcSanity(const KURL& src, ExecutionContext* context) {
  // Invalid URLs will be rejected early on.
  DCHECK(src.IsValid());

  if (!src.ProtocolIs(url::kHttpScheme) && !src.ProtocolIs(url::kHttpsScheme) &&
      !src.ProtocolIs(url::kDataScheme) && !src.ProtocolIs(url::kBlobScheme)) {
    context->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        "MediaImage src can only be of http/https/data/blob scheme: " +
            src.GetString()));
    return false;
  }

  DCHECK(src.GetString().Is8Bit());
  if (src.GetString().length() > url::kMaxURLChars) {
    context->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning,
        "MediaImage src exceeds maximum URL length: " + src.GetString()));
    return false;
  }
  return true;
}

// Sanitize MediaImage and do mojo serialization. Returns null when
// |image.src()| is bad.
media_session::mojom::blink::MediaImagePtr SanitizeMediaImageAndConvertToMojo(
    const MediaImage* image,
    ExecutionContext* context) {
  media_session::mojom::blink::MediaImagePtr mojo_image;

  KURL url = KURL(image->src());
  if (!CheckMediaImageSrcSanity(url, context))
    return mojo_image;

  mojo_image = media_session::mojom::blink::MediaImage::New();
  mojo_image->src = url;
  mojo_image->type = image->type().Left(kMaxImageTypeLength);
  for (const auto& web_size :
       WebIconSizesParser::ParseIconSizes(image->sizes())) {
    mojo_image->sizes.push_back(web_size);
    if (mojo_image->sizes.size() == kMaxNumberOfImageSizes) {
      context->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          "The number of MediaImage sizes exceeds the upper limit. "
          "All remaining MediaImage will be ignored"));
      break;
    }
  }
  return mojo_image;
}

}  // anonymous namespace

blink::mojom::blink::SpecMediaMetadataPtr
MediaMetadataSanitizer::SanitizeAndConvertToMojo(const MediaMetadata* metadata,
                                                 ExecutionContext* context) {
  if (!metadata)
    return blink::mojom::blink::SpecMediaMetadataPtr();

  blink::mojom::blink::SpecMediaMetadataPtr mojo_metadata(
      blink::mojom::blink::SpecMediaMetadata::New());

  mojo_metadata->title = metadata->title().Left(kMaxStringLength);
  mojo_metadata->artist = metadata->artist().Left(kMaxStringLength);
  mojo_metadata->album = metadata->album().Left(kMaxStringLength);

  for (const MediaImage* image : metadata->artwork()) {
    media_session::mojom::blink::MediaImagePtr mojo_image =
        SanitizeMediaImageAndConvertToMojo(image, context);
    if (!mojo_image.is_null())
      mojo_metadata->artwork.push_back(std::move(mojo_image));
    if (mojo_metadata->artwork.size() == kMaxNumberOfMediaImages) {
      context->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kWarning,
          "The number of MediaImage sizes exceeds the upper limit. "
          "All remaining MediaImage will be ignored"));
      break;
    }
  }
  return mojo_metadata;
}

}  // namespace blink

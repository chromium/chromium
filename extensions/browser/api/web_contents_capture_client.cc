// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_contents_capture_client.h"

#include <optional>

#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/syslog_logging.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

namespace extensions {

using api::extension_types::ImageDetails;

WebContentsCaptureClient::CaptureResult WebContentsCaptureClient::CaptureAsync(
    WebContents* web_contents,
    const ImageDetails* image_details,
    base::OnceCallback<void(const SkBitmap&)> callback) {
  // TODO(crbug.com/41135213): Account for fullscreen render widget?
  RenderWidgetHostView* const view =
      web_contents ? web_contents->GetRenderWidgetHostView() : nullptr;
  if (!view)
    return FAILURE_REASON_VIEW_INVISIBLE;

  // Check for screenshot capture restrictions.
  ScreenshotAccess screenshot_access = GetScreenshotAccess(web_contents);
  if (screenshot_access == ScreenshotAccess::kDisabledByPreferences)
    return FAILURE_REASON_SCREEN_SHOTS_DISABLED;
  if (screenshot_access == ScreenshotAccess::kDisabledByDlp)
    return FAILURE_REASON_SCREEN_SHOTS_DISABLED_BY_DLP;

  // The default format and quality setting used when encoding jpegs.
  const api::extension_types::ImageFormat kDefaultFormat =
      api::extension_types::ImageFormat::kJpeg;
  const int kDefaultQuality = 90;

  image_format_ = kDefaultFormat;
  image_quality_ = kDefaultQuality;

  if (image_details) {
    if (image_details->format != api::extension_types::ImageFormat::kNone) {
      image_format_ = image_details->format;
    }
    if (image_details->quality)
      image_quality_ = *image_details->quality;
  }

  view->CopyFromSurface(gfx::Rect(),  // Copy entire surface area.
                        gfx::Size(),  // Result contains device-level detail.
                        std::move(callback));

#if BUILDFLAG(IS_CHROMEOS)
  SYSLOG(INFO) << "Screenshot taken";
#endif

  return OK;
}

void WebContentsCaptureClient::CopyFromSurfaceComplete(const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    OnCaptureFailure(FAILURE_REASON_READBACK_FAILED);
  } else {
    OnCaptureSuccess(bitmap);
  }
}

std::optional<std::string> WebContentsCaptureClient::EncodeBitmap(
    const SkBitmap& bitmap) {
  const bool should_discard_alpha = !ClientAllowsTransparency();
  std::optional<std::vector<uint8_t>> data;
  std::string mime_type;
  switch (image_format_) {
    case api::extension_types::ImageFormat::kJpeg:
      data = gfx::JPEGCodec::Encode(bitmap, image_quality_);
      mime_type = kMimeTypeJpeg;
      break;
    case api::extension_types::ImageFormat::kPng:
      data = gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, should_discard_alpha);
      mime_type = kMimeTypePng;
      break;
    default:
      NOTREACHED() << "Invalid image format.";
  }

  if (!data) {
    return std::nullopt;
  }

  return base::StrCat(
      {"data:", mime_type, ";base64,", base::Base64Encode(data.value())});
}

}  // namespace extensions

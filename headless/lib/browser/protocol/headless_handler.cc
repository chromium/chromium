// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/headless_handler.h"

#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/switches.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"  // nogncheck http://crbug.com/1227378
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/image/image.h"

namespace headless {
namespace protocol {

using HeadlessExperimental::ScreenshotParams;

namespace {

constexpr int kDefaultScreenshotQuality = 80;

using BitmapEncoder =
    base::RepeatingCallback<bool(const SkBitmap& bitmap,
                                 std::vector<uint8_t>& output)>;

bool EncodeBitmapAsPngSlow(const SkBitmap& bitmap,
                           std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsPngSlow");
  return gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
}

bool EncodeBitmapAsPngFast(const SkBitmap& bitmap,
                           std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsPngFast");
  return gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output);
}

bool EncodeBitmapAsJpeg(int quality,
                        const SkBitmap& bitmap,
                        std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsJpeg");
  return gfx::JPEGCodec::Encode(bitmap, quality, &output);
}

bool EncodeBitmapAsWebp(int quality,
                        const SkBitmap& bitmap,
                        std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsWebp");
  return gfx::WebpCodec::Encode(bitmap, quality, &output);
}

absl::variant<protocol::Response, BitmapEncoder>
GetEncoder(const std::string& format, int quality, bool optimize_for_speed) {
  if (quality < 0 || quality > 100) {
    return Response::InvalidParams(
        "screenshot.quality has to be in range 0..100");
  }
  if (format == ScreenshotParams::FormatEnum::Png) {
    return base::BindRepeating(optimize_for_speed ? EncodeBitmapAsPngFast
                                                  : EncodeBitmapAsPngSlow);
  }
  if (format == ScreenshotParams::FormatEnum::Jpeg)
    return base::BindRepeating(&EncodeBitmapAsJpeg, quality);
  if (format == ScreenshotParams::FormatEnum::Webp)
    return base::BindRepeating(&EncodeBitmapAsWebp, quality);
  return protocol::Response::InvalidParams("Invalid image format");
}

void OnBeginFrameFinished(
    BitmapEncoder encoder,
    std::unique_ptr<HeadlessHandler::BeginFrameCallback> callback,
    bool has_damage,
    std::unique_ptr<SkBitmap> bitmap,
    std::string error_message) {
  if (!error_message.empty()) {
    callback->sendFailure(Response::ServerError(std::move(error_message)));
    return;
  }
  if (!bitmap || bitmap->drawsNothing()) {
    callback->sendSuccess(has_damage, Maybe<protocol::Binary>());
    return;
  }
  std::vector<uint8_t> bytes;
  bool success = encoder.Run(*bitmap, bytes);
  DCHECK(success || bytes.empty());
  callback->sendSuccess(has_damage, Binary::fromVector(bytes));
}

}  // namespace

HeadlessHandler::HeadlessHandler(HeadlessBrowserImpl* browser,
                                 content::WebContents* web_contents)
    : browser_(browser), web_contents_(web_contents) {}

HeadlessHandler::~HeadlessHandler() {}

void HeadlessHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ =
      std::make_unique<HeadlessExperimental::Frontend>(dispatcher->channel());
  HeadlessExperimental::Dispatcher::wire(dispatcher, this);
}

Response HeadlessHandler::Enable() {
  return Response::Success();
}

Response HeadlessHandler::Disable() {
  return Response::Success();
}

void HeadlessHandler::BeginFrame(Maybe<double> in_frame_time_ticks,
                                 Maybe<double> in_interval,
                                 Maybe<bool> in_no_display_updates,
                                 Maybe<ScreenshotParams> screenshot,
                                 std::unique_ptr<BeginFrameCallback> callback) {
  HeadlessWebContentsImpl* headless_contents =
      HeadlessWebContentsImpl::From(browser_, web_contents_);
  if (!headless_contents->begin_frame_control_enabled()) {
    callback->sendFailure(Response::ServerError(
        "Command is only supported if BeginFrameControl is enabled."));
    return;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kRunAllCompositorStagesBeforeDraw)) {
    callback->sendFailure(Response::ServerError(
        "Command is only supported with "
        "--run-all-compositor-stages-before-draw, see "
        "https://goo.gle/chrome-headless-rendering for more info."));
    return;
  }

  base::TimeTicks frame_time_ticks;
  base::TimeDelta interval;
  bool no_display_updates = in_no_display_updates.value_or(false);

  if (in_frame_time_ticks.has_value()) {
    frame_time_ticks =
        base::TimeTicks() + base::Milliseconds(in_frame_time_ticks.value());
  } else {
    frame_time_ticks = base::TimeTicks::Now();
  }

  if (in_interval.has_value()) {
    double interval_double = in_interval.value();
    if (interval_double <= 0) {
      callback->sendFailure(
          Response::InvalidParams("interval has to be greater than 0"));
      return;
    }
    interval = base::Milliseconds(interval_double);
  } else {
    interval = viz::BeginFrameArgs::DefaultInterval();
  }

  base::TimeTicks deadline = frame_time_ticks + interval;

  BitmapEncoder encoder;
  if (screenshot.has_value()) {
    ScreenshotParams& params = screenshot.value();
    auto encoder_or_response =
        GetEncoder(params.GetFormat(ScreenshotParams::FormatEnum::Png),
                   params.GetQuality(kDefaultScreenshotQuality),
                   params.GetOptimizeForSpeed(false));
    if (absl::holds_alternative<protocol::Response>(encoder_or_response)) {
      callback->sendFailure(absl::get<protocol::Response>(encoder_or_response));
      return;
    }
    encoder = absl::get<BitmapEncoder>(std::move(encoder_or_response));
  }

  const bool capture_screenshot = !!encoder;
  headless_contents->BeginFrame(
      frame_time_ticks, deadline, interval, no_display_updates,
      capture_screenshot,
      base::BindOnce(&OnBeginFrameFinished, std::move(encoder),
                     std::move(callback)));
}

}  // namespace protocol
}  // namespace headless

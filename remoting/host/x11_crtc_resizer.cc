// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/x11_crtc_resizer.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/gfx/x/future.h"

namespace {

constexpr auto kInvalidMode = static_cast<x11::RandR::Mode>(0);
constexpr auto kDisabledCrtc = static_cast<x11::RandR::Crtc>(0);

}  // namespace

namespace remoting {

X11CrtcResizer::CrtcInfo::CrtcInfo() = default;
X11CrtcResizer::CrtcInfo::CrtcInfo(x11::RandR::Crtc crtc,
                                   int16_t x,
                                   int16_t y,
                                   uint16_t width,
                                   uint16_t height,
                                   x11::RandR::Mode mode,
                                   x11::RandR::Rotation rotation,
                                   std::vector<x11::RandR::Output>&& outputs)
    : crtc(crtc),
      x(x),
      y(y),
      width(width),
      height(height),
      mode(mode),
      rotation(rotation),
      outputs(outputs) {}
X11CrtcResizer::CrtcInfo::CrtcInfo(const X11CrtcResizer::CrtcInfo&) = default;
X11CrtcResizer::CrtcInfo::CrtcInfo(X11CrtcResizer::CrtcInfo&&) = default;
X11CrtcResizer::CrtcInfo& X11CrtcResizer::CrtcInfo::operator=(
    const X11CrtcResizer::CrtcInfo&) = default;
X11CrtcResizer::CrtcInfo& X11CrtcResizer::CrtcInfo::operator=(
    X11CrtcResizer::CrtcInfo&&) = default;
X11CrtcResizer::CrtcInfo::~CrtcInfo() = default;

X11CrtcResizer::X11CrtcResizer(
    x11::RandR::GetScreenResourcesCurrentReply* resources,
    x11::RandR* randr)
    : resources_(resources), randr_(randr) {}

X11CrtcResizer::~X11CrtcResizer() = default;

void X11CrtcResizer::FetchActiveCrtcs() {
  active_crtcs_.clear();
  x11::Time config_timestamp = resources_->config_timestamp;
  for (const auto& crtc : resources_->crtcs) {
    auto response = randr_->GetCrtcInfo({crtc, config_timestamp}).Sync();
    if (!response)
      continue;
    if (response->outputs.empty())
      continue;

    active_crtcs_.emplace_back(
        crtc, response->x, response->y, response->width, response->height,
        response->mode, response->rotation, std::move(response->outputs));
  }
}

x11::RandR::Crtc X11CrtcResizer::GetCrtcForOutput(
    x11::RandR::Output output) const {
  // This implementation assumes an output is attached to only one CRTC. If
  // there are multiple CRTCs for the output, only the first will be returned,
  // but this should never occur with Xorg+video-dummy.
  auto iter =
      base::ranges::find_if(active_crtcs_, [output](const CrtcInfo& crtc_info) {
        return base::Contains(crtc_info.outputs, output);
      });
  if (iter == active_crtcs_.end()) {
    return kDisabledCrtc;
  }
  return iter->crtc;
}

void X11CrtcResizer::DisableCrtc(x11::RandR::Crtc crtc) {
  x11::Time config_timestamp = resources_->config_timestamp;
  randr_->SetCrtcConfig({
      .crtc = crtc,
      .timestamp = x11::Time::CurrentTime,
      .config_timestamp = config_timestamp,
      .x = 0,
      .y = 0,
      .mode = kInvalidMode,
      .rotation = x11::RandR::Rotation::Rotate_0,
      .outputs = {},
  });
}

void X11CrtcResizer::UpdateActiveCrtcs(x11::RandR::Crtc crtc,
                                       x11::RandR::Mode mode,
                                       const webrtc::DesktopSize& new_size) {
  // Find |crtc| in |active_crtcs_| and adjust its mode and size.
  auto iter = base::ranges::find(active_crtcs_, crtc, &CrtcInfo::crtc);

  // |crtc| was returned by GetCrtcForOutput() so it should definitely be in
  // the list.
  DCHECK(iter != active_crtcs_.end());

  iter->mode = mode;
  iter->width = new_size.width();
  iter->height = new_size.height();

  // Mark it as changed so that ApplyActiveCrtcs() will apply the new |mode|.
  // The |width| and |height| are only used for computing the bounding-box,
  // they are not used by ApplyActiveCrtcs().
  iter->changed = true;

  // TODO(crbug.com/1326339): Adjust the xy-offsets to avoid overlaps.
}

void X11CrtcResizer::DisableChangedCrtcs() {
  base::ranges::for_each(active_crtcs_, [this](const CrtcInfo& crtc_info) {
    if (crtc_info.changed) {
      DisableCrtc(crtc_info.crtc);
    }
  });
}

webrtc::DesktopSize X11CrtcResizer::GetBoundingBox() const {
  webrtc::DesktopSize result;
  base::ranges::for_each(active_crtcs_, [&result](const CrtcInfo& crtc_info) {
    int32_t width = crtc_info.x + crtc_info.width;
    int32_t height = crtc_info.y + crtc_info.height;
    result.set(std::max(result.width(), width),
               std::max(result.height(), height));
  });
  return result;
}

void X11CrtcResizer::ApplyActiveCrtcs() {
  base::ranges::for_each(active_crtcs_, [this](const CrtcInfo& crtc_info) {
    if (crtc_info.changed) {
      x11::Time config_timestamp = resources_->config_timestamp;
      randr_->SetCrtcConfig({
          .crtc = crtc_info.crtc,
          .timestamp = x11::Time::CurrentTime,
          .config_timestamp = config_timestamp,
          .x = crtc_info.x,
          .y = crtc_info.y,
          .mode = crtc_info.mode,
          .rotation = crtc_info.rotation,
          .outputs = crtc_info.outputs,
      });
    }
  });
}

}  // namespace remoting

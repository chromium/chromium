// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory_selector.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace media {

// WARNING: The returned names are used as part of UMA names. Do NOT change
// existing return names.
std::string GetRendererName(RendererType renderer_type) {
  switch (renderer_type) {
    case RendererType::kDefault:
      return "RendererImpl";
    case RendererType::kMojo:
      return "MojoRenderer";
    case RendererType::kMediaPlayer:
      return "MediaPlayerRenderer";
    case RendererType::kCourier:
      return "CourierRenderer";
    case RendererType::kFlinging:
      return "FlingingRenderer";
    case RendererType::kCast:
      return "CastRenderer";
    case RendererType::kMediaFoundation:
      return "MediaFoundationRenderer";
    case RendererType::kRemoting:
      return "RemotingRenderer";  // media::remoting::Receiver
    case RendererType::kCastStreaming:
      return "CastStreamingRenderer";
    case RendererType::kContentEmbedderDefined:
      return "EmbedderDefined";
    default:
      NOTREACHED();
      return "RendererType created through invalid static_cast";
  }
}

RendererFactorySelector::RendererFactorySelector() = default;

RendererFactorySelector::~RendererFactorySelector() = default;

void RendererFactorySelector::AddBaseFactory(
    RendererType type,
    std::unique_ptr<RendererFactory> factory) {
  DVLOG(1) << __func__ << ": type=" << GetRendererName(type);
  DCHECK(!base_renderer_type_) << "At most one base factory!";

  AddFactory(type, std::move(factory));
  SetBaseRendererType(type);
}

void RendererFactorySelector::AddConditionalFactory(
    RendererType type,
    std::unique_ptr<RendererFactory> factory,
    ConditionalFactoryCB callback) {
  DCHECK(factory);
  DCHECK(callback);
  DCHECK(!conditional_factories_.count(type))
      << "At most one conditional factory for a given type!";

  conditional_factories_.emplace(type, callback);
  AddFactory(type, std::move(factory));
}

void RendererFactorySelector::AddFactory(
    RendererType type,
    std::unique_ptr<RendererFactory> factory) {
  DCHECK(factory);
  DCHECK(!factories_.count(type));
  DVLOG(2) << __func__ << ": type=" << GetRendererName(type);
  factories_[type] = std::move(factory);
}

void RendererFactorySelector::SetBaseRendererType(RendererType type) {
  DCHECK(factories_.count(type));
  base_renderer_type_ = type;
}

RendererType RendererFactorySelector::GetCurrentRendererType() {
  for (const auto& entry : conditional_factories_) {
    if (entry.second.Run())
      return entry.first;
  }

  return base_renderer_type_.value();
}

RendererFactory* RendererFactorySelector::GetCurrentFactory() {
  RendererType current_renderer_type = GetCurrentRendererType();

  DVLOG(1) << __func__ << " Selecting factory type: "
           << GetRendererName(current_renderer_type);
  auto* current_factory = factories_[current_renderer_type].get();
  DCHECK(current_factory);

  return current_factory;
}

#if BUILDFLAG(IS_ANDROID)
void RendererFactorySelector::StartRequestRemotePlayStateCB(
    RequestRemotePlayStateChangeCB callback_request) {
  DCHECK(!remote_play_state_change_cb_request_);
  remote_play_state_change_cb_request_ = std::move(callback_request);
}

void RendererFactorySelector::SetRemotePlayStateChangeCB(
    RemotePlayStateChangeCB callback) {
  DCHECK(remote_play_state_change_cb_request_);
  std::move(remote_play_state_change_cb_request_).Run(std::move(callback));
}
#endif

}  // namespace media

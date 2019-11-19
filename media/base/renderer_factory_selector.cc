// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory_selector.h"

#include "base/logging.h"

namespace media {

RendererFactorySelector::RendererFactorySelector() = default;

RendererFactorySelector::~RendererFactorySelector() = default;

void RendererFactorySelector::AddBaseFactory(
    FactoryType type,
    std::unique_ptr<RendererFactory> factory) {
  DCHECK(!base_factory_type_) << "At most one base factory!";

  AddFactory(type, std::move(factory));
  SetBaseFactoryType(type);
}

void RendererFactorySelector::AddConditionalFactory(
    FactoryType type,
    std::unique_ptr<RendererFactory> factory,
    ConditionalFactoryCB callback) {
  DCHECK(factory);
  DCHECK(callback);
  DCHECK(!conditional_factory_types_.count(type))
      << "At most one conditional factory for a given type!";

  conditional_factory_types_.emplace(type, callback);
  AddFactory(type, std::move(factory));
}

void RendererFactorySelector::AddFactory(
    FactoryType type,
    std::unique_ptr<RendererFactory> factory) {
  DCHECK(factory);
  DCHECK(!factories_[type]);
  factories_[type] = std::move(factory);
}

void RendererFactorySelector::SetBaseFactoryType(FactoryType type) {
  DCHECK(factories_[type]);
  base_factory_type_ = type;
}

RendererFactorySelector::FactoryType
RendererFactorySelector::GetCurrentFactoryType() {
  for (const auto& entry : conditional_factory_types_) {
    if (entry.second.Run())
      return entry.first;
  }

  return base_factory_type_.value();
}

RendererFactory* RendererFactorySelector::GetCurrentFactory() {
  FactoryType current_factory_type = GetCurrentFactoryType();

  DVLOG(1) << __func__ << " Selecting factory type: " << current_factory_type;
  auto* current_factory = factories_[current_factory_type].get();
  DCHECK(current_factory);

  return current_factory;
}

#if defined(OS_ANDROID)
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

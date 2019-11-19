// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_
#define MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

#include "base/callback.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "media/base/media_status.h"
#include "media/base/renderer_factory.h"

namespace media {

// RendererFactorySelector owns RendererFactory instances used within WMPI.
// Its purpose is to aggregate the signals and centralize the logic behind
// choosing which RendererFactory should be used when creating a new Renderer.
//
// There are 3 categories of factories: base, conditional and other, which can
// be added by AddBaseFactory(), AddConditionalFactory() and AddFactory()
// respectively.
//
// The current factory is selected as:
// - If a conditional factory exists and the condition is met, use the
//   conditional factory.
// - Else use the base factory.
//
// Notes:
// - One and at most one base factory must be set.
// - The base factory can be changed by calling SetBaseFactoryType().
// - Multiple conditional factories are supported but there should be at most
//   one conditional factory for any factory type. If multiple conditions are
//   met, it's up to the implementation detail which factory will be returned.

class MEDIA_EXPORT RendererFactorySelector {
 public:
  using ConditionalFactoryCB = base::RepeatingCallback<bool()>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum FactoryType {
    DEFAULT = 0,       // DefaultRendererFactory.
    MOJO = 1,          // MojoRendererFactory.
    MEDIA_PLAYER = 2,  // MediaPlayerRendererClientFactory.
    COURIER = 3,       // CourierRendererFactory.
    FLINGING = 4,      // FlingingRendererClientFactory.
    CAST = 5,          // CastRendererClientFactory.
    FACTORY_TYPE_MAX = CAST,
  };

  RendererFactorySelector();
  ~RendererFactorySelector();

  // See file level comments above.
  void AddBaseFactory(FactoryType type,
                      std::unique_ptr<RendererFactory> factory);
  void AddConditionalFactory(FactoryType type,
                             std::unique_ptr<RendererFactory> factory,
                             ConditionalFactoryCB callback);
  void AddFactory(FactoryType type, std::unique_ptr<RendererFactory> factory);

  // Sets the base factory to be returned, when there are no signals telling us
  // to select any specific factory.
  // NOTE: |type| can be different than FactoryType::DEFAULT. DEFAULT is used to
  // identify the DefaultRendererFactory, not to indicate that a factory should
  // be used by default.
  void SetBaseFactoryType(FactoryType type);

  // Returns the type of the factory that GetCurrentFactory() would return.
  // NOTE: SetBaseFactoryType() must be called before calling this method.
  FactoryType GetCurrentFactoryType();

  // Updates |current_factory_| if necessary, and returns its value.
  // NOTE: SetBaseFactoryType() must be called before calling this method.
  RendererFactory* GetCurrentFactory();

#if defined(OS_ANDROID)
  // Starts a request to receive a RemotePlayStateChangeCB, to be fulfilled
  // later by passing a request via SetRemotePlayStateChangeCB().
  // NOTE: There should be no pending request (this new one would overwrite it).
  void StartRequestRemotePlayStateCB(
      RequestRemotePlayStateChangeCB callback_request);

  // Fulfills a request initiated by StartRequestRemotePlayStateCB().
  // NOTE: There must be a pending request.
  void SetRemotePlayStateChangeCB(RemotePlayStateChangeCB callback);
#endif

 private:
  base::Optional<FactoryType> base_factory_type_;

  // Use a map to avoid duplicate entires for the same FactoryType.
  std::map<FactoryType, ConditionalFactoryCB> conditional_factory_types_;

  RequestRemotePlayStateChangeCB remote_play_state_change_cb_request_;

  std::unique_ptr<RendererFactory> factories_[FACTORY_TYPE_MAX + 1];
  DISALLOW_COPY_AND_ASSIGN(RendererFactorySelector);
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_FACTORY_H_

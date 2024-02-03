// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_
#define MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

#include <map>
#include <optional>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/media_status.h"
#include "media/base/renderer.h"
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
// - The base factory can be changed by calling SetBaseRendererType().
// - Multiple conditional factories are supported but there should be at most
//   one conditional factory for any factory type. If multiple conditions are
//   met, it's up to the implementation detail which factory will be returned.
class MEDIA_EXPORT RendererFactorySelector {
 public:
  using ConditionalFactoryCB = base::RepeatingCallback<bool()>;

  RendererFactorySelector();

  RendererFactorySelector(const RendererFactorySelector&) = delete;
  RendererFactorySelector& operator=(const RendererFactorySelector&) = delete;

  virtual ~RendererFactorySelector();

  // See file level comments above.
  void AddBaseFactory(RendererType type,
                      std::unique_ptr<RendererFactory> factory);
  void AddConditionalFactory(RendererType type,
                             std::unique_ptr<RendererFactory> factory,
                             ConditionalFactoryCB callback);
  void AddFactory(RendererType type, std::unique_ptr<RendererFactory> factory);

  // Sets the base factory to be returned, when there are no signals telling us
  // to select any specific factory.
  void SetBaseRendererType(RendererType type);

  // Returns the type of the Renderer for what GetCurrentFactory() would return.
  // NOTE: SetBaseRendererType() must be called before calling this method.
  virtual RendererType GetCurrentRendererType();

  // Updates |current_factory_| if necessary, and returns its value.
  // NOTE: SetBaseRendererType() must be called before calling this method.
  virtual RendererFactory* GetCurrentFactory();

#if BUILDFLAG(IS_ANDROID)
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
  std::optional<RendererType> base_renderer_type_;

  // Use a map to avoid duplicate entries for the same RendererType.
  std::map<RendererType, ConditionalFactoryCB> conditional_factories_;

  RequestRemotePlayStateChangeCB remote_play_state_change_cb_request_;

  std::map<RendererType, std::unique_ptr<RendererFactory>> factories_;
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_
#define MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

#include <map>

#include "base/callback.h"
#include "build/build_config.h"
#include "media/base/media_status.h"
#include "media/base/renderer_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

// Types of media::Renderer.
// WARNING: These values are reported to metrics. Entries should not be
// renumbered and numeric values should not be reused. When adding new entries,
// also update media::mojom::RendererType & tools/metrics/histograms/enums.xml.
enum class RendererType {
  kDefault = 0,          // DefaultRendererFactory
  kMojo = 1,             // MojoRendererFactory
  kMediaPlayer = 2,      // MediaPlayerRendererClientFactory
  kCourier = 3,          // CourierRendererFactory
  kFlinging = 4,         // FlingingRendererClientFactory
  kCast = 5,             // CastRendererClientFactory
  kMediaFoundation = 6,  // MediaFoundationRendererClientFactory
  // kFuchsia = 7,       // Deprecated
  kRemoting = 8,       // RemotingRendererFactory for remoting::Receiver
  kCastStreaming = 9,  // PlaybackCommandForwardingRendererFactory
  kContentEmbedderDefined = 10,  // Defined by the content embedder
  kMaxValue = kContentEmbedderDefined,
};

// Get the name of the Renderer for `renderer_type`. The returned name could be
// the actual Renderer class name or a descriptive name.
std::string MEDIA_EXPORT GetRendererName(RendererType renderer_type);

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
  // NOTE: |type| can be different than RendererType::kDefault. kDefault is used
  // to identify the DefaultRendererFactory, not to indicate that a factory
  // should be used by default.
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
  absl::optional<RendererType> base_renderer_type_;

  // Use a map to avoid duplicate entries for the same RendererType.
  std::map<RendererType, ConditionalFactoryCB> conditional_factories_;

  RequestRemotePlayStateChangeCB remote_play_state_change_cb_request_;

  std::map<RendererType, std::unique_ptr<RendererFactory>> factories_;
};

}  // namespace media

#endif  // MEDIA_BASE_RENDERER_FACTORY_SELECTOR_H_

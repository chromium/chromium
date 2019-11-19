// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace blink {
class WebInputEvent;
}

namespace url {
class Origin;
}

namespace content {

class PepperWebPluginImpl;
class RenderFrameImpl;

class CONTENT_EXPORT PluginInstanceThrottlerImpl
    : public PluginInstanceThrottler {
 public:
  explicit PluginInstanceThrottlerImpl(
      RenderFrame::RecordPeripheralDecision record_decision);

  ~PluginInstanceThrottlerImpl() override;

  // PluginInstanceThrottler implementation:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsThrottled() override;
  bool IsHiddenForPlaceholder() override;
  void MarkPluginEssential(PowerSaverUnthrottleMethod method) override;
  void SetHiddenForPlaceholder(bool hidden) override;
  PepperWebPluginImpl* GetWebPlugin() override;
  const gfx::Size& GetSize() override;
  void NotifyAudioThrottled() override;

  void SetWebPlugin(PepperWebPluginImpl* web_plugin);

  bool needs_representative_keyframe() const {
    return state_ == THROTTLER_STATE_AWAITING_KEYFRAME;
  }

  bool power_saver_enabled() const {
    return state_ != THROTTLER_STATE_MARKED_ESSENTIAL;
  }

  void Initialize(RenderFrameImpl* frame,
                  const url::Origin& content_origin,
                  const std::string& plugin_module_name,
                  const gfx::Size& unobscured_size);

  // Called when the plugin flushes it's graphics context. Supplies the
  // throttler with a candidate to use as the representative keyframe.
  void OnImageFlush(const SkBitmap& bitmap);

  // Returns true if |event| was handled and shouldn't be further processed.
  bool ConsumeInputEvent(const blink::WebInputEvent& event);

 private:
  friend class PluginInstanceThrottlerImplTest;

  enum ThrottlerState {
    // Plugin has been found to be peripheral, Plugin Power Saver is enabled,
    // and throttler is awaiting a representative keyframe.
    THROTTLER_STATE_AWAITING_KEYFRAME,
    // A representative keyframe has been chosen and the plugin is throttled.
    THROTTLER_STATE_PLUGIN_THROTTLED,
    // Plugin instance has been marked essential.
    THROTTLER_STATE_MARKED_ESSENTIAL,
  };

  // Maximum number of frames to examine for a suitable keyframe. After that, we
  // simply suspend the plugin where it's at. Chosen arbitrarily.
  static const int kMaximumFramesToExamine;

  void AudioThrottledFrameTimeout();
  void EngageThrottle();

  RenderFrame::RecordPeripheralDecision record_decision_;

  ThrottlerState state_;

  bool is_hidden_for_placeholder_;

  PepperWebPluginImpl* web_plugin_;

  // Holds a reference to the last received frame. This doesn't actually copy
  // the pixel data, but rather increments the reference count to the pixels.
  SkBitmap last_received_frame_;

  // Number of frames we've examined to find a keyframe.
  int frames_examined_;

  // Plugin's unobscured dimensions as of initialization.
  gfx::Size unobscured_size_;

  // Video plugins with throttled audio often stop generating frames.
  // This timer is so we don't wait forever for candidate poster frames.
  bool audio_throttled_;
  base::DelayTimer audio_throttled_frame_timeout_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<PluginInstanceThrottlerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginInstanceThrottlerImpl);
};
}

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_INSTANCE_THROTTLER_IMPL_H_

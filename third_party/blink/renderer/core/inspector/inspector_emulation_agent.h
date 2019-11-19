// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_EMULATION_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_EMULATION_AGENT_H_

#include "base/macros.h"
#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/Emulation.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/timezone/timezone_controller.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"

namespace blink {

class DocumentLoader;
class ResourceRequest;
class WebLocalFrameImpl;
class WebViewImpl;
enum class ResourceType : uint8_t;
struct FetchInitiatorInfo;

namespace protocol {
namespace DOM {
class RGBA;
}  // namespace DOM
}  // namespace protocol

class CORE_EXPORT InspectorEmulationAgent final
    : public InspectorBaseAgent<protocol::Emulation::Metainfo> {
 public:
  explicit InspectorEmulationAgent(WebLocalFrameImpl*);
  ~InspectorEmulationAgent() override;

  // protocol::Dispatcher::EmulationCommandHandler implementation.
  protocol::Response resetPageScaleFactor() override;
  protocol::Response setPageScaleFactor(double) override;
  protocol::Response setScriptExecutionDisabled(bool value) override;
  protocol::Response setScrollbarsHidden(bool hidden) override;
  protocol::Response setDocumentCookieDisabled(bool disabled) override;
  protocol::Response setTouchEmulationEnabled(
      bool enabled,
      protocol::Maybe<int> max_touch_points) override;
  protocol::Response setEmulatedMedia(
      protocol::Maybe<String> media,
      protocol::Maybe<protocol::Array<protocol::Emulation::MediaFeature>>
          features) override;
  protocol::Response setCPUThrottlingRate(double) override;
  protocol::Response setFocusEmulationEnabled(bool) override;
  protocol::Response setVirtualTimePolicy(
      const String& policy,
      protocol::Maybe<double> virtual_time_budget_ms,
      protocol::Maybe<int> max_virtual_time_task_starvation_count,
      protocol::Maybe<bool> wait_for_navigation,
      protocol::Maybe<double> initial_virtual_time,
      double* virtual_time_ticks_base_ms) override;
  protocol::Response setTimezoneOverride(const String& timezone_id) override;
  protocol::Response setNavigatorOverrides(const String& platform) override;
  protocol::Response setDefaultBackgroundColorOverride(
      protocol::Maybe<protocol::DOM::RGBA>) override;
  protocol::Response setDeviceMetricsOverride(
      int width,
      int height,
      double device_scale_factor,
      bool mobile,
      protocol::Maybe<double> scale,
      protocol::Maybe<int> screen_width,
      protocol::Maybe<int> screen_height,
      protocol::Maybe<int> position_x,
      protocol::Maybe<int> position_y,
      protocol::Maybe<bool> dont_set_visible_size,
      protocol::Maybe<protocol::Emulation::ScreenOrientation>,
      protocol::Maybe<protocol::Page::Viewport>) override;
  protocol::Response clearDeviceMetricsOverride() override;
  protocol::Response setUserAgentOverride(
      const String& user_agent,
      protocol::Maybe<String> accept_language,
      protocol::Maybe<String> platform) override;

  // InspectorInstrumentation API
  void ApplyAcceptLanguageOverride(String* accept_lang);
  void ApplyUserAgentOverride(String* user_agent);
  void FrameStartedLoading(LocalFrame*);
  void PrepareRequest(DocumentLoader*,
                      ResourceRequest&,
                      const FetchInitiatorInfo&,
                      ResourceType);

  // InspectorBaseAgent overrides.
  protocol::Response disable() override;
  void Restore() override;

  void Trace(blink::Visitor*) override;

 private:
  WebViewImpl* GetWebViewImpl();
  protocol::Response AssertPage();
  void VirtualTimeBudgetExpired();
  void InnerEnable();

  struct PendingVirtualTimePolicy {
    PageScheduler::VirtualTimePolicy policy;
    base::Optional<double> virtual_time_budget_ms;
    base::Optional<int> max_virtual_time_task_starvation_count;
  };
  void ApplyVirtualTimePolicy(const PendingVirtualTimePolicy& new_policy);

  Member<WebLocalFrameImpl> web_local_frame_;
  base::TimeTicks virtual_time_base_ticks_;

  std::unique_ptr<TimeZoneController::TimeZoneOverride> timezone_override_;

  // Supports a virtual time policy change scheduled to occur after any
  // navigation has started.
  base::Optional<PendingVirtualTimePolicy> pending_virtual_time_policy_;
  bool enabled_ = false;

  InspectorAgentState::Bytes default_background_color_override_rgba_;
  InspectorAgentState::Boolean script_execution_disabled_;
  InspectorAgentState::Boolean scrollbars_hidden_;
  InspectorAgentState::Boolean document_cookie_disabled_;
  InspectorAgentState::Boolean touch_event_emulation_enabled_;
  InspectorAgentState::Integer max_touch_points_;
  InspectorAgentState::String emulated_media_;
  InspectorAgentState::StringMap emulated_media_features_;
  InspectorAgentState::String navigator_platform_override_;
  InspectorAgentState::String user_agent_override_;
  InspectorAgentState::String accept_language_override_;
  InspectorAgentState::Double virtual_time_budget_;
  InspectorAgentState::Double virtual_time_budget_initial_offset_;
  InspectorAgentState::Double initial_virtual_time_;
  InspectorAgentState::Double virtual_time_offset_;
  InspectorAgentState::String virtual_time_policy_;
  InspectorAgentState::Integer virtual_time_task_starvation_count_;
  InspectorAgentState::Boolean wait_for_navigation_;
  InspectorAgentState::Boolean emulate_focus_;
  InspectorAgentState::String timezone_id_override_;
  DISALLOW_COPY_AND_ASSIGN(InspectorEmulationAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_EMULATION_AGENT_H_

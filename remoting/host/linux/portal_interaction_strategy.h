// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_PORTAL_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_LINUX_PORTAL_INTERACTION_STRATEGY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/linux/portal_remote_desktop_session.h"

namespace remoting {

class PortalInteractionStrategy : public DesktopInteractionStrategy {
 public:
  PortalInteractionStrategy();
  ~PortalInteractionStrategy() override;

  PortalInteractionStrategy(const PortalInteractionStrategy&) = delete;
  PortalInteractionStrategy& operator=(const PortalInteractionStrategy&) =
      delete;

  // DesktopInteractionStrategy interface.
  std::unique_ptr<ActionExecutor> CreateActionExecutor() override;
  std::unique_ptr<AudioCapturer> CreateAudioCapturer() override;
  std::unique_ptr<InputInjector> CreateInputInjector() override;
  std::unique_ptr<DesktopResizer> CreateDesktopResizer() override;
  std::unique_ptr<DesktopCapturer> CreateVideoCapturer(
      webrtc::ScreenId id) override;
  std::unique_ptr<protocol::MouseCursorMonitor> CreateMouseCursorMonitor()
      override;
  std::unique_ptr<KeyboardLayoutMonitor> CreateKeyboardLayoutMonitor(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
      override;
  std::unique_ptr<ActiveDisplayMonitor> CreateActiveDisplayMonitor(
      base::RepeatingCallback<void(webrtc::ScreenId)> callback) override;
  std::unique_ptr<DesktopDisplayInfoMonitor> CreateDisplayInfoMonitor()
      override;
  std::unique_ptr<LocalInputMonitor> CreateLocalInputMonitor() override;
  std::unique_ptr<CurtainMode> CreateCurtainMode(
      base::WeakPtr<ClientSessionControl> client_session_control) override;

 private:
  friend class PortalInteractionStrategyFactory;

  using InitCallbackSignature = void(base::expected<void, std::string>);
  using InitCallback = base::OnceCallback<InitCallbackSignature>;

  void Init(InitCallback callback);
  void OnInitResult(InitCallback callback,
                    base::expected<void, std::string> result);

  raw_ptr<PortalRemoteDesktopSession> remote_desktop_ =
      PortalRemoteDesktopSession::GetInstance();
  base::WeakPtrFactory<PortalInteractionStrategy> weak_ptr_factory_{this};
};

class PortalInteractionStrategyFactory
    : public DesktopInteractionStrategyFactory {
 public:
  PortalInteractionStrategyFactory();
  ~PortalInteractionStrategyFactory() override;
  void Create(const DesktopEnvironmentOptions& options,
              CreateCallback callback) override;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_PORTAL_INTERACTION_STRATEGY_H_

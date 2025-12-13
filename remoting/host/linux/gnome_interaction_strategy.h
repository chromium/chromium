// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "remoting/host/desktop_capturer_proxy.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/linux/gnome_capture_stream_manager.h"
#include "remoting/host/linux/gnome_remote_desktop_session.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace remoting {

class GnomeInteractionStrategy : public DesktopInteractionStrategy,
                                 public GnomeCaptureStreamManager::Observer {
 public:
  GnomeInteractionStrategy(const GnomeInteractionStrategy&) = delete;
  GnomeInteractionStrategy& operator=(const GnomeInteractionStrategy&) = delete;
  ~GnomeInteractionStrategy() override;

  // Correspond to the equivalent methods on DesktopEnvironment.
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

  // Used by DesktopEnvironment implementations.
  std::unique_ptr<DesktopDisplayInfoMonitor> CreateDisplayInfoMonitor()
      override;
  std::unique_ptr<LocalInputMonitor> CreateLocalInputMonitor() override;
  std::unique_ptr<CurtainMode> CreateCurtainMode(
      base::WeakPtr<ClientSessionControl> client_session_control) override;

 private:
  friend class GnomeDisplayInfoLoader;
  friend class GnomeInteractionStrategyFactory;
  friend class GnomeDesktopDisplayInfoMonitor;

  using InitCallbackSignature = void(base::expected<void, std::string>);
  using InitCallback = base::OnceCallback<InitCallbackSignature>;
  explicit GnomeInteractionStrategy(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  void Init(InitCallback callback);
  void OnInitResult(InitCallback callback,
                    base::expected<void, std::string> result);

  // GnomeCaptureStreamManager::Observer overrides.
  void OnPipewireCaptureStreamAdded(
      base::WeakPtr<CaptureStream> stream) override;

  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  raw_ptr<GnomeRemoteDesktopSession> remote_desktop_session_ GUARDED_BY_CONTEXT(
      sequence_checker_) = GnomeRemoteDesktopSession::GetInstance();
  gvariant::ObjectPath stream_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  GnomeCaptureStreamManager::Observer::Subscription
      capture_stream_manager_subscription_
          GUARDED_BY_CONTEXT(sequence_checker_);

  // Map to allow capturers pending initialization to be initialized after the
  // corresponding pipewire stream is created, which may happen before or after
  // the capturer is created.
  base::flat_map<webrtc::ScreenId, base::WeakPtr<DesktopCapturerProxy>>
      pending_desktop_capturer_proxies_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GnomeInteractionStrategy> weak_ptr_factory_{this};
};

class GnomeInteractionStrategyFactory
    : public DesktopInteractionStrategyFactory {
 public:
  explicit GnomeInteractionStrategyFactory(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  ~GnomeInteractionStrategyFactory() override;
  void Create(const DesktopEnvironmentOptions& options,
              CreateCallback callback) override;

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_

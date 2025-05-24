// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_
#define REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_

#include <memory>
#include <string>
#include <tuple>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/desktop_interaction_strategy.h"
#include "remoting/host/linux/ei_sender_session.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gdbus_fd_list.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/pipewire_capture_stream.h"

namespace remoting {

class GnomeInteractionStrategy : public DesktopInteractionStrategy {
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
  std::unique_ptr<webrtc::MouseCursorMonitor> CreateMouseCursorMonitor()
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

 private:
  friend class GnomeInteractionStrategyFactory;
  friend class GnomeInputInjector;

  using InitCallback =
      base::OnceCallback<void(base::expected<void, std::string>)>;
  explicit GnomeInteractionStrategy(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  template <typename SuccessType, typename String>
  GDBusConnectionRef::CallCallback<SuccessType> CheckResultAndContinue(
      void (GnomeInteractionStrategy::*success_method)(SuccessType),
      String&& error_context);
  void OnInitError(std::string_view what, Loggable why);
  void Init(InitCallback callback);
  void OnConnectionCreated(GDBusConnectionRef connection);
  void OnSessionCreated(std::tuple<gvariant::ObjectPath> args);
  void OnGotSessionId(std::string session_id);
  void OnScreenCastSessionCreated(std::tuple<gvariant::ObjectPath> args);
  void OnSessionStarted(std::tuple<>);
  void OnEisFd(std::pair<std::tuple<GDBusFdList::Handle>, GDBusFdList> args);
  void OnEiSession(std::unique_ptr<EiSenderSession> ei_session);
  void OnStreamCreated(std::tuple<gvariant::ObjectPath> args);
  void OnStreamParameters(GVariantRef<"a{sv}"> parameters);
  void OnStreamStarted(std::tuple<> args);
  void OnPipeWireStreamAdded(std::string mapping_id,
                             std::tuple<std::uint32_t> args);

  void InjectKeyEvent(const protocol::KeyEvent& event);
  void InjectTextEvent(const protocol::TextEvent& event);
  void InjectMouseEvent(const protocol::MouseEvent& event);

  GDBusConnectionRef connection_ GUARDED_BY_CONTEXT(sequence_checker_);
  InitCallback init_callback_;
  gvariant::ObjectPath session_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath screencast_session_path_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<EiSenderSession> ei_session_
      GUARDED_BY_CONTEXT(sequence_checker_);
  gvariant::ObjectPath stream_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<GDBusConnectionRef::SignalSubscription> stream_added_signal_
      GUARDED_BY_CONTEXT(sequence_checker_);
  PipewireCaptureStream capture_stream_ GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<GnomeInteractionStrategy> weak_ptr_factory_;
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
  static void OnSessionInit(
      std::unique_ptr<GnomeInteractionStrategy> session,
      base::OnceCallback<void(std::unique_ptr<DesktopInteractionStrategy>)>
          callback,
      base::expected<void, std::string> result);
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_INTERACTION_STRATEGY_H_

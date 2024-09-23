// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_INJECTOR_X11_H_
#define REMOTING_HOST_INPUT_INJECTOR_X11_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/input_injector_constants_linux.h"
#include "remoting/host/linux/x11_character_injector.h"
#include "remoting/host/linux/x11_keyboard_impl.h"
#include "remoting/host/linux/x11_util.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/gfx/x/xproto.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/host/chromeos/point_transformer.h"
#endif

namespace remoting {

// A class to generate events on X11.
class InputInjectorX11 : public InputInjector {
 public:
  explicit InputInjectorX11(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  InputInjectorX11(const InputInjectorX11&) = delete;
  InputInjectorX11& operator=(const InputInjectorX11&) = delete;
  ~InputInjectorX11() override;
  void Init();
  // Clipboard stub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;
  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  // The actual implementation resides in InputInjectorX11::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    explicit Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

    Core(const Core&) = delete;

    Core& operator=(const Core&) = delete;

    void Init();

    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const protocol::ClipboardEvent& event);

    // Mirrors the InputStub interface.
    void InjectKeyEvent(const protocol::KeyEvent& event);
    void InjectTextEvent(const protocol::TextEvent& event);
    void InjectMouseEvent(const protocol::MouseEvent& event);

    // Mirrors the InputInjector interface.
    void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);
    void Stop();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void InitClipboard();

    // Queries whether keyboard auto-repeat is globally enabled. This is used
    // to decide whether to temporarily disable then restore this setting. If
    // auto-repeat has already been disabled, this class should leave it
    // untouched.
    bool IsAutoRepeatEnabled();

    // Enables or disables keyboard auto-repeat globally.
    void SetAutoRepeatEnabled(bool enabled);

    // Check if the given scan code is caps lock or num lock.
    bool IsLockKey(x11::KeyCode keycode);

    // Sets the keyboard lock states to those provided.
    void SetLockStates(std::optional<bool> caps_lock,
                       std::optional<bool> num_lock);

    void InjectScrollWheelClicks(int button, int count);

    // Compensates for global button mappings and resets the XTest device
    // mapping.
    void InitMouseButtonMap();

    int MouseButtonToX11ButtonNumber(protocol::MouseEvent::MouseButton button);
    int HorizontalScrollWheelToX11ButtonNumber(int dx);
    int VerticalScrollWheelToX11ButtonNumber(int dy);

    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
    std::set<int> pressed_keys_;
    webrtc::DesktopVector latest_mouse_position_ =
        webrtc::DesktopVector(-1, -1);
    float wheel_ticks_x_ = 0;
    float wheel_ticks_y_ = 0;
    base::Time latest_tick_y_event_;

    // The direction of the last scroll event that resulted in at least one
    // "tick" being injected.
    ScrollDirection latest_tick_y_direction_ = ScrollDirection::NONE;

    // X11 graphics context. Must only be accessed on the input thread.
    raw_ptr<x11::Connection> connection_;

    // Number of buttons we support.
    // Left, Right, Middle, VScroll Up/Down, HScroll Left/Right, back, forward.
    static const int kNumPointerButtons = 9;

    int pointer_button_map_[kNumPointerButtons];
#if BUILDFLAG(IS_CHROMEOS_ASH)
    PointTransformer point_transformer_;
#endif
    std::unique_ptr<Clipboard> clipboard_;
    std::unique_ptr<X11CharacterInjector> character_injector_;
    bool saved_auto_repeat_enabled_ = false;
  };
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_INJECTOR_X11_H_

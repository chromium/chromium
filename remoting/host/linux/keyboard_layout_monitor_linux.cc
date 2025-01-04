// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include <gdk/gdk.h>

#include <optional>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/linux/keyboard_layout_monitor_utils.h"
#include "remoting/host/linux/keyboard_layout_monitor_wayland.h"
#include "remoting/host/linux/wayland_utils.h"
#include "remoting/proto/control.pb.h"
#include "ui/base/glib/scoped_gsignal.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/future.h"
#include "ui/gfx/x/xkb.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace remoting {

namespace {

class KeyboardLayoutMonitorLinux;

// Deletes a pointer on the main GTK+ thread.
class GtkThreadDeleter {
 public:
  template <typename T>
  void operator()(T* p) const;

 private:
  template <typename T>
  static gboolean DeleteOnGtkThread(gpointer p);
};

// Can be constructed on any thread, but must be started and destroyed on the
// main GTK+ thread (i.e., the GLib global default main context).
class GdkLayoutMonitorOnGtkThread : public x11::EventObserver {
 public:
  GdkLayoutMonitorOnGtkThread(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<KeyboardLayoutMonitorLinux> weak_ptr);

  // Must be called on GTK Thread
  ~GdkLayoutMonitorOnGtkThread() override;
  void Start();

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  void QueryLayout();
  void OnKeysChanged(GdkKeymap* keymap);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<KeyboardLayoutMonitorLinux> weak_ptr_;
  raw_ptr<x11::Connection> connection_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> controller_;
  raw_ptr<GdkDisplay> display_ = nullptr;
  raw_ptr<GdkKeymap> keymap_ = nullptr;
  int current_group_ = 0;
  ScopedGSignal signal_;
};

class KeyboardLayoutMonitorLinux : public KeyboardLayoutMonitor {
 public:
  explicit KeyboardLayoutMonitorLinux(
      base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback);

  ~KeyboardLayoutMonitorLinux() override;

  void Start() override;

  // Used by GdkLayoutMonitorOnGtkThread.
  using KeyboardLayoutMonitor::kSupportedKeys;
  void OnLayoutChanged(const protocol::KeyboardLayout& new_layout);

 private:
  static gboolean StartLayoutMonitorOnGtkThread(gpointer gdk_layout_monitor);

  base::RepeatingCallback<void(const protocol::KeyboardLayout&)>
      layout_changed_callback_;
  // Must be deleted on the GTK thread.
  std::unique_ptr<GdkLayoutMonitorOnGtkThread, GtkThreadDeleter>
      gdk_layout_monitor_;
  base::WeakPtrFactory<KeyboardLayoutMonitorLinux> weak_ptr_factory_;
};

template <typename T>
void GtkThreadDeleter::operator()(T* p) const {
  g_idle_add(DeleteOnGtkThread<T>, p);
}

// static
template <typename T>
gboolean GtkThreadDeleter::DeleteOnGtkThread(gpointer p) {
  delete static_cast<T*>(p);
  // Only run once.
  return G_SOURCE_REMOVE;
}

GdkLayoutMonitorOnGtkThread::GdkLayoutMonitorOnGtkThread(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<KeyboardLayoutMonitorLinux> weak_ptr)
    : task_runner_(std::move(task_runner)), weak_ptr_(std::move(weak_ptr)) {}

GdkLayoutMonitorOnGtkThread::~GdkLayoutMonitorOnGtkThread() {
  DCHECK(g_main_context_is_owner(g_main_context_default()));
  if (display_) {
    connection_->RemoveEventObserver(this);
  }
}

void GdkLayoutMonitorOnGtkThread::Start() {
  DCHECK(g_main_context_is_owner(g_main_context_default()));
  display_ = gdk_display_get_default();
  if (!display_) {
    LOG(WARNING) << "No default display for layout monitoring.";
    return;
  }

  // The keymap, as GDK sees it, is the collection of all (up to 4) enabled
  // keyboard layouts, which it and XKB refer to as "groups". The "keys-changed"
  // signal is only fired when this keymap, containing all enabled layouts, is
  // changed, such as by adding, removing, or rearranging layouts. Annoyingly,
  // it does *not* fire when the active group (layout) is changed. Indeed, for
  // whatever reason, GDK doesn't provide *any* method of obtaining or listening
  // for changes to the active group as far as I can tell, even though it tracks
  // the active group internally so it can emit a "direction-changed" signal
  // when switching between groups with different writing directions. As a
  // result, we have to use Xkb directly to get and monitor that information,
  // which is a pain.
  connection_ = x11::Connection::Get();
  auto& xkb = connection_->xkb();
  if (xkb.present()) {
    constexpr auto kXkbAllStateComponentsMask =
        static_cast<x11::Xkb::StatePart>(0x3fff);
    xkb.SelectEvents({
        .deviceSpec =
            static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd),
        .affectWhich = x11::Xkb::EventType::StateNotify,
        .affectState = kXkbAllStateComponentsMask,
        .stateDetails = x11::Xkb::StatePart::GroupState,
    });
    connection_->Flush();
  }
  connection_->AddEventObserver(this);

  keymap_ = gdk_keymap_get_for_display(display_);
  signal_ = ScopedGSignal(
      keymap_, "keys-changed",
      base::BindRepeating(&GdkLayoutMonitorOnGtkThread::OnKeysChanged,
                          base::Unretained(this)));
  QueryLayout();
}

void GdkLayoutMonitorOnGtkThread::OnEvent(const x11::Event& event) {
  if (event.As<x11::MappingNotifyEvent>() ||
      event.As<x11::Xkb::NewKeyboardNotifyEvent>()) {
    QueryLayout();
  } else if (auto* notify = event.As<x11::Xkb::StateNotifyEvent>()) {
    int new_group = notify->baseGroup + notify->latchedGroup +
                    static_cast<int16_t>(notify->lockedGroup);
    if (new_group != current_group_) {
      QueryLayout();
    }
  }
}

void GdkLayoutMonitorOnGtkThread::QueryLayout() {
  protocol::KeyboardLayout layout_message;

  auto shift_modifier = x11::KeyButMask::Shift;
  auto numlock_modifier = x11::KeyButMask::Mod2;
  auto altgr_modifier = x11::KeyButMask::Mod5;

  bool have_altgr = false;

  auto req = connection_->xkb().GetState(
      {static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd)});
  if (auto reply = req.Sync()) {
    current_group_ = static_cast<int>(reply->group);
  }

  for (ui::DomCode key : KeyboardLayoutMonitorLinux::kSupportedKeys) {
    // Skip single-layout IME keys for now, as they are always present in the
    // keyboard map but not present on most keyboards. Client-side IME is likely
    // more convenient, anyway.
    // TODO(rkjnsn): Figure out how to show these keys only when relevant.
    if (key == ui::DomCode::LANG1 || key == ui::DomCode::LANG2 ||
        key == ui::DomCode::CONVERT || key == ui::DomCode::NON_CONVERT ||
        key == ui::DomCode::KANA_MODE) {
      continue;
    }

    std::uint32_t usb_code = ui::KeycodeConverter::DomCodeToUsbKeycode(key);
    int keycode = ui::KeycodeConverter::DomCodeToNativeKeycode(key);

    // Insert entry for USB code. It's fine to overwrite if we somehow process
    // the same USB code twice, since the actions will be the same.
    auto& key_actions =
        *(*layout_message.mutable_keys())[usb_code].mutable_actions();

    for (int shift_level = 0; shift_level < 8; ++shift_level) {
      // Don't bother capturing higher shift levels if there's no configured way
      // to access them.
      if ((shift_level & 2 && !have_altgr) || (shift_level & 4)) {
        continue;
      }

      // Always consider NumLock set and CapsLock unset for now.
      auto modifiers = numlock_modifier |
                       (shift_level & 1 ? shift_modifier : x11::KeyButMask{}) |
                       (shift_level & 2 ? altgr_modifier : x11::KeyButMask{});
      guint keyval = 0;
      gdk_keymap_translate_keyboard_state(
          keymap_, keycode, static_cast<GdkModifierType>(modifiers),
          current_group_, &keyval, nullptr, nullptr, nullptr);
      if (keyval == 0) {
        continue;
      }

      guint32 unicode = gdk_keyval_to_unicode(keyval);
      if (unicode != 0) {
        switch (unicode) {
          case 0x08:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::BACKSPACE);
            break;
          case 0x09:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::TAB);
            break;
          case 0x0D:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::ENTER);
            break;
          case 0x1B:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::ESCAPE);
            break;
          case 0x7F:
            key_actions[shift_level].set_function(
                protocol::LayoutKeyFunction::DELETE_);
            break;
          default:
            std::string utf8;
            base::WriteUnicodeCharacter(unicode, &utf8);
            key_actions[shift_level].set_character(utf8);
        }
        continue;
      }

      const char* dead_key_utf8 = DeadKeyToUtf8String(keyval);
      if (dead_key_utf8) {
        key_actions[shift_level].set_character(dead_key_utf8);
        continue;
      }

      if (keyval == GDK_KEY_Num_Lock || keyval == GDK_KEY_Caps_Lock) {
        // Don't include Num Lock or Caps Lock until we decide if / how we want
        // to handle them.
        // TODO(rkjnsn): Determine if supporting Num Lock / Caps Lock provides
        // enough utility to warrant support by the soft keyboard.
        continue;
      }

      protocol::LayoutKeyFunction function = KeyvalToFunction(keyval);
      if (function == protocol::LayoutKeyFunction::ALT_GR) {
        have_altgr = true;
      }
      key_actions[shift_level].set_function(function);
    }

    if (key_actions.empty()) {
      layout_message.mutable_keys()->erase(usb_code);
    }
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&KeyboardLayoutMonitorLinux::OnLayoutChanged,
                                weak_ptr_, std::move(layout_message)));
}

void GdkLayoutMonitorOnGtkThread::OnKeysChanged(GdkKeymap* keymap) {
  QueryLayout();
}

KeyboardLayoutMonitorLinux::KeyboardLayoutMonitorLinux(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback)
    : layout_changed_callback_(std::move(callback)), weak_ptr_factory_(this) {}

KeyboardLayoutMonitorLinux::~KeyboardLayoutMonitorLinux() = default;

void KeyboardLayoutMonitorLinux::Start() {
  DCHECK(!gdk_layout_monitor_);
  gdk_layout_monitor_.reset(new GdkLayoutMonitorOnGtkThread(
      base::SequencedTaskRunner::GetCurrentDefault(),
      weak_ptr_factory_.GetWeakPtr()));
  g_idle_add(StartLayoutMonitorOnGtkThread, gdk_layout_monitor_.get());
}

void KeyboardLayoutMonitorLinux::OnLayoutChanged(
    const protocol::KeyboardLayout& new_layout) {
  layout_changed_callback_.Run(new_layout);
}

// static
gboolean KeyboardLayoutMonitorLinux::StartLayoutMonitorOnGtkThread(
    gpointer gdk_layout_monitor) {
  static_cast<GdkLayoutMonitorOnGtkThread*>(gdk_layout_monitor)->Start();
  // Only run once.
  return G_SOURCE_REMOVE;
}

}  // namespace

// static
std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  if (IsRunningWayland()) {
    return std::make_unique<KeyboardLayoutMonitorWayland>(std::move(callback));
  }
  return std::make_unique<KeyboardLayoutMonitorLinux>(std::move(callback));
}

}  // namespace remoting

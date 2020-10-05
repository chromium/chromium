// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/keyboard_layout_monitor.h"

#include <gdk/gdk.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "remoting/proto/control.pb.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/x/x11.h"
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
class GdkLayoutMonitorOnGtkThread : public ui::XEventDispatcher {
 public:
  GdkLayoutMonitorOnGtkThread(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<KeyboardLayoutMonitorLinux> weak_ptr);

  // Must be called on GTK Thread
  ~GdkLayoutMonitorOnGtkThread() override;
  void Start();

 private:
  // ui::XEventDispatcher:
  bool DispatchXEvent(x11::Event* event) override;

  void QueryLayout();
  CHROMEG_CALLBACK_0(GdkLayoutMonitorOnGtkThread,
                     void,
                     OnKeysChanged,
                     GdkKeymap*);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<KeyboardLayoutMonitorLinux> weak_ptr_;
  x11::Connection* connection_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> controller_;
  GdkDisplay* display_ = nullptr;
  GdkKeymap* keymap_ = nullptr;
  int current_group_ = 0;
  gulong handler_id_ = 0;
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

protocol::LayoutKeyFunction KeyvalToFunction(guint keyval);
const char* DeadKeyToUtf8String(guint keyval);

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
  if (handler_id_) {
    g_signal_handler_disconnect(keymap_, handler_id_);
    ui::X11EventSource::GetInstance()->RemoveXEventDispatcher(this);
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
  if (xkb.UseExtension({x11::Xkb::major_version, x11::Xkb::minor_version})
          .Sync()) {
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
  ui::X11EventSource::GetInstance()->AddXEventDispatcher(this);

  keymap_ = gdk_keymap_get_for_display(display_);
  handler_id_ = g_signal_connect(keymap_, "keys-changed",
                                 G_CALLBACK(OnKeysChangedThunk), this);
  QueryLayout();
}

bool GdkLayoutMonitorOnGtkThread::DispatchXEvent(x11::Event* event) {
  if (event->As<x11::MappingNotifyEvent>() ||
      event->As<x11::Xkb::NewKeyboardNotifyEvent>()) {
    QueryLayout();
  } else if (auto* notify = event->As<x11::Xkb::StateNotifyEvent>()) {
    int new_group = notify->baseGroup + notify->latchedGroup +
                    static_cast<int16_t>(notify->lockedGroup);
    if (new_group != current_group_)
      QueryLayout();
    return true;
  }
  return false;
}

void GdkLayoutMonitorOnGtkThread::QueryLayout() {
  protocol::KeyboardLayout layout_message;

  auto shift_modifier = x11::KeyButMask::Shift;
  auto numlock_modifier = x11::KeyButMask::Mod2;
  auto altgr_modifier = x11::KeyButMask::Mod5;

  bool have_altgr = false;

  auto req = connection_->xkb().GetState(
      {static_cast<x11::Xkb::DeviceSpec>(x11::Xkb::Id::UseCoreKbd)});
  if (auto reply = req.Sync())
    current_group_ = static_cast<int>(reply->group);

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
      base::SequencedTaskRunnerHandle::Get(), weak_ptr_factory_.GetWeakPtr()));
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

// GDK doesn't return unicode value for dead keys, so we map them here.
const char* DeadKeyToUtf8String(guint keyval) {
  // Some of these have spacing forms and some don't. For consistency, these are
  // all of the form space+combining character. This file is a good resource to
  // figure out what the various dead keys are supposed to do:
  // https://cgit.freedesktop.org/xorg/lib/libX11/plain/nls/en_US.UTF-8/Compose.pre
  // See also ui/events/keycodes/keysym_to_unicode.cc, which does a similar
  // mapping.
  switch (keyval) {
    case GDK_KEY_dead_grave:
      return " \xcc\x80";  // U+0300
    case GDK_KEY_dead_acute:
      return " \xcc\x81";  // U+0301
    case GDK_KEY_dead_circumflex:
      return " \xcc\x82";  // U+0302
    case GDK_KEY_dead_tilde:
      return " \xcc\x83";  // U+0303
    case GDK_KEY_dead_macron:
      return " \xcc\x84";  // U+0304
    case GDK_KEY_dead_breve:
      return " \xcc\x86";  // U+0306
    case GDK_KEY_dead_abovedot:
      return " \xcc\x87";  // U+0307
    case GDK_KEY_dead_diaeresis:
      return " \xcc\x88";  // U+0308
    case GDK_KEY_dead_abovering:
      return " \xcc\x8a";  // U+030A
    case GDK_KEY_dead_doubleacute:
      return " \xcc\x8b";  // U+030B
    case GDK_KEY_dead_caron:
      return " \xcc\x8c";  // U+030C
    case GDK_KEY_dead_cedilla:
      return " \xcc\xa7";  // U+0327
    case GDK_KEY_dead_ogonek:
      return " \xcc\xa8";  // U+0328
    case GDK_KEY_dead_iota:
      return " \xcd\x85";  // U+0345
    case GDK_KEY_dead_voiced_sound:
      return " \xe3\x82\x99";  // U+3099
    case GDK_KEY_dead_semivoiced_sound:
      return " \xe3\x82\x9a";  // U+309A
    case GDK_KEY_dead_belowdot:
      return " \xcc\xa3";  // U+0323
    case GDK_KEY_dead_hook:
      return " \xcc\x89";  // U+0309
    case GDK_KEY_dead_horn:
      return " \xcc\x9b";  // U+031B
    case GDK_KEY_dead_stroke:
      return " \xcc\xb7";  // U+0337
    case GDK_KEY_dead_abovecomma:
      return " \xcc\x93";  // U+0313
    case GDK_KEY_dead_abovereversedcomma:
      return " \xcc\x94";  // U+0314
    case GDK_KEY_dead_doublegrave:
      return " \xcc\x8f";  // U+030F
    case GDK_KEY_dead_belowring:
      return " \xcc\xa5";  // U+0325
    case GDK_KEY_dead_belowmacron:
      return " \xcc\xb1";  // U+0331
    case GDK_KEY_dead_belowcircumflex:
      return " \xcc\xad";  // U+032D
    case GDK_KEY_dead_belowtilde:
      return " \xcc\xb0";  // U+0330
    case GDK_KEY_dead_belowbreve:
      return " \xcc\xae";  // U+032E
    case GDK_KEY_dead_belowdiaeresis:
      return " \xcc\xa4";  // U+0324
    case GDK_KEY_dead_invertedbreve:
      return " \xcc\x91";  // U+0311
    case GDK_KEY_dead_belowcomma:
      return " \xcc\xa6";  // U+0326
    case GDK_KEY_dead_currency:
      // This one is a bit different: instead of adding a diacritic to a
      // character, it allows entering of various currency symbols. E.g.,
      // dead_currency+e generates the euro sign (€). Combining this dead key
      // with space generates the general currency symbol.
      return "\xc2\xa4";  // Currency symbol (¤), U+00A4
    // I can't find any information about what these do. There is, e.g.,
    // "combining latin small letter a" (U+0363) that places a tiny "a" above
    // the preceding character (and similar for the other lower-case letters),
    // but no equivalent capital versions. These don't show up in Compose.pre,
    // so they're probably not used much, if at all.
    // case GDK_KEY_dead_a:
    // case GDK_KEY_dead_A:
    // case GDK_KEY_dead_e:
    // case GDK_KEY_dead_E:
    // case GDK_KEY_dead_i:
    // case GDK_KEY_dead_I:
    // case GDK_KEY_dead_o:
    // case GDK_KEY_dead_O:
    // case GDK_KEY_dead_u:
    // case GDK_KEY_dead_U:
    // case GDK_KEY_dead_small_schwa:
    // case GDK_KEY_dead_capital_schwa:
    case GDK_KEY_dead_greek:
      // Like dead_currency above, this key is used to generate different
      // symbols entirely (greek letters, in this case). E.g., dead_greek+a
      // generates Greek alpha (α). Combining this dead key with space generates
      // the micro sign (µ), a distinct code point from Greek mu (μ), which is
      // generated by dead_greek+m.
      return "\xce\xbc";  // Micro symbol (μ), U+00B5
    default:
      return nullptr;
  }
}

protocol::LayoutKeyFunction KeyvalToFunction(guint keyval) {
  switch (keyval) {
    case GDK_KEY_Control_L:
    case GDK_KEY_Control_R:
      return protocol::LayoutKeyFunction::CONTROL;
    case GDK_KEY_Alt_L:
    case GDK_KEY_Alt_R:
      return protocol::LayoutKeyFunction::ALT;
    case GDK_KEY_Shift_L:
    case GDK_KEY_Shift_R:
      return protocol::LayoutKeyFunction::SHIFT;
    case GDK_KEY_Super_L:
    case GDK_KEY_Super_R:
      return protocol::LayoutKeyFunction::META;
    case GDK_KEY_ISO_Level3_Shift:
      return protocol::LayoutKeyFunction::ALT_GR;
    case GDK_KEY_ISO_Level5_Shift:
      return protocol::LayoutKeyFunction::MOD5;
    case GDK_KEY_Multi_key:
      return protocol::LayoutKeyFunction::COMPOSE;
    case GDK_KEY_Num_Lock:
      return protocol::LayoutKeyFunction::NUM_LOCK;
    case GDK_KEY_Caps_Lock:
      return protocol::LayoutKeyFunction::CAPS_LOCK;
    case GDK_KEY_Scroll_Lock:
      return protocol::LayoutKeyFunction::SCROLL_LOCK;
    case GDK_KEY_BackSpace:
      return protocol::LayoutKeyFunction::BACKSPACE;
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      return protocol::LayoutKeyFunction::ENTER;
    case GDK_KEY_Tab:           // Unshifted
    case GDK_KEY_ISO_Left_Tab:  // Shifted
    case GDK_KEY_KP_Tab:
      return protocol::LayoutKeyFunction::TAB;
    case GDK_KEY_Insert:
    case GDK_KEY_KP_Insert:
      return protocol::LayoutKeyFunction::INSERT;
    case GDK_KEY_Delete:
    case GDK_KEY_KP_Delete:
      return protocol::LayoutKeyFunction::DELETE_;
    case GDK_KEY_Home:
    case GDK_KEY_KP_Home:
      return protocol::LayoutKeyFunction::HOME;
    case GDK_KEY_End:
    case GDK_KEY_KP_End:
      return protocol::LayoutKeyFunction::END;
    case GDK_KEY_Page_Up:
    case GDK_KEY_KP_Page_Up:
      return protocol::LayoutKeyFunction::PAGE_UP;
    case GDK_KEY_Page_Down:
    case GDK_KEY_KP_Page_Down:
      return protocol::LayoutKeyFunction::PAGE_DOWN;
    case GDK_KEY_Clear:
      return protocol::LayoutKeyFunction::CLEAR;
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      return protocol::LayoutKeyFunction::ARROW_UP;
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      return protocol::LayoutKeyFunction::ARROW_DOWN;
    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
      return protocol::LayoutKeyFunction::ARROW_LEFT;
    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
      return protocol::LayoutKeyFunction::ARROW_RIGHT;
    case GDK_KEY_F1:
    case GDK_KEY_KP_F1:
      return protocol::LayoutKeyFunction::F1;
    case GDK_KEY_F2:
    case GDK_KEY_KP_F2:
      return protocol::LayoutKeyFunction::F2;
    case GDK_KEY_F3:
    case GDK_KEY_KP_F3:
      return protocol::LayoutKeyFunction::F3;
    case GDK_KEY_F4:
    case GDK_KEY_KP_F4:
      return protocol::LayoutKeyFunction::F4;
    case GDK_KEY_F5:
      return protocol::LayoutKeyFunction::F5;
    case GDK_KEY_F6:
      return protocol::LayoutKeyFunction::F6;
    case GDK_KEY_F7:
      return protocol::LayoutKeyFunction::F7;
    case GDK_KEY_F8:
      return protocol::LayoutKeyFunction::F8;
    case GDK_KEY_F9:
      return protocol::LayoutKeyFunction::F9;
    case GDK_KEY_F10:
      return protocol::LayoutKeyFunction::F10;
    case GDK_KEY_F11:
      return protocol::LayoutKeyFunction::F11;
    case GDK_KEY_F12:
      return protocol::LayoutKeyFunction::F12;
    case GDK_KEY_F13:
      return protocol::LayoutKeyFunction::F13;
    case GDK_KEY_F14:
      return protocol::LayoutKeyFunction::F14;
    case GDK_KEY_F15:
      return protocol::LayoutKeyFunction::F15;
    case GDK_KEY_F16:
      return protocol::LayoutKeyFunction::F16;
    case GDK_KEY_F17:
      return protocol::LayoutKeyFunction::F17;
    case GDK_KEY_F18:
      return protocol::LayoutKeyFunction::F18;
    case GDK_KEY_F19:
      return protocol::LayoutKeyFunction::F19;
    case GDK_KEY_F20:
      return protocol::LayoutKeyFunction::F20;
    case GDK_KEY_F21:
      return protocol::LayoutKeyFunction::F21;
    case GDK_KEY_F22:
      return protocol::LayoutKeyFunction::F22;
    case GDK_KEY_F23:
      return protocol::LayoutKeyFunction::F23;
    case GDK_KEY_F24:
      return protocol::LayoutKeyFunction::F24;
    case GDK_KEY_Escape:
      return protocol::LayoutKeyFunction::ESCAPE;
    case GDK_KEY_Menu:
      return protocol::LayoutKeyFunction::CONTEXT_MENU;
    case GDK_KEY_Pause:
      return protocol::LayoutKeyFunction::PAUSE;
    case GDK_KEY_Print:    // Unshifted
    case GDK_KEY_Sys_Req:  // Shifted
      return protocol::LayoutKeyFunction::PRINT_SCREEN;
    case GDK_KEY_Zenkaku_Hankaku:  // Unshifted
    case GDK_KEY_Kanji:            // Shifted
      return protocol::LayoutKeyFunction::HANKAKU_ZENKAKU_KANJI;
    case GDK_KEY_Henkan:
      return protocol::LayoutKeyFunction::HENKAN;
    case GDK_KEY_Muhenkan:
      return protocol::LayoutKeyFunction::MUHENKAN;
    case GDK_KEY_Hiragana_Katakana:  // Unshifted
    case GDK_KEY_Romaji:             // Shifted
      return protocol::KATAKANA_HIRAGANA_ROMAJI;
    case GDK_KEY_Eisu_toggle:
      return protocol::LayoutKeyFunction::EISU;
    case GDK_KEY_Hangul:
      return protocol::LayoutKeyFunction::HAN_YEONG;
    case GDK_KEY_Hangul_Hanja:
      return protocol::LayoutKeyFunction::HANJA;
    default:
      return protocol::LayoutKeyFunction::UNKNOWN;
  }
}

}  // namespace

std::unique_ptr<KeyboardLayoutMonitor> KeyboardLayoutMonitor::Create(
    base::RepeatingCallback<void(const protocol::KeyboardLayout&)> callback,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner) {
  return std::make_unique<KeyboardLayoutMonitorLinux>(std::move(callback));
}

}  // namespace remoting

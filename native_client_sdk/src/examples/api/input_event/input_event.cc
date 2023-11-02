// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C headers
#include <stdio.h>

// C++ headers
#include <sstream>
#include <string>

// PPAPI headers
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef PostMessage
#undef PostMessage
#endif

namespace {

std::string ModifierToString(uint32_t modifier) {
  std::string s;
  if (modifier & PP_INPUTEVENT_MODIFIER_SHIFTKEY) {
    s += "shift ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CONTROLKEY) {
    s += "ctrl ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ALTKEY) {
    s += "alt ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_METAKEY) {
    s += "meta ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISKEYPAD) {
    s += "keypad ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT) {
    s += "autorepeat ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
    s += "left-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) {
    s += "middle-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN) {
    s += "right-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY) {
    s += "caps-lock ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_NUMLOCKKEY) {
    s += "num-lock ";
  }
  return s;
}

std::string MouseButtonToString(PP_InputEvent_MouseButton button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
      return "None";
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return "Left";
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return "Middle";
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return "Right";
    default:
      std::ostringstream stream;
      stream << "Unrecognized (" << static_cast<int32_t>(button) << ")";
      return stream.str();
  }
}

std::string TouchKindToString(PP_InputEvent_Type kind) {
  switch (kind) {
    case PP_INPUTEVENT_TYPE_TOUCHSTART:
      return "Start";
    case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      return "Move";
    case PP_INPUTEVENT_TYPE_TOUCHEND:
      return "End";
    case PP_INPUTEVENT_TYPE_TOUCHCANCEL:
      return "Cancel";
    default:
      std::ostringstream stream;
      stream << "Unrecognized (" << static_cast<int32_t>(kind) << ")";
      return stream.str();
  }
}

}  // namespace

class InputEventInstance : public pp::Instance {
 public:
  explicit InputEventInstance(PP_Instance instance)
      : pp::Instance(instance) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL |
                       PP_INPUTEVENT_CLASS_TOUCH);
    RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  }

  bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    return true;
  }

  /// Clicking outside of the instance's bounding box
  /// will create a DidChangeFocus event (the NaCl instance is
  /// out of focus). Clicking back inside the instance's
  /// bounding box will create another DidChangeFocus event
  /// (the NaCl instance is back in focus). The default is
  /// that the instance is out of focus.
  void DidChangeFocus(bool focus) {
    std::ostringstream stream;
    stream << "DidChangeFocus:" << " focus:" << focus;
    PostMessage(stream.str());
  }

  /// Scrolling the mouse wheel causes a DidChangeView event.
  void DidChangeView(const pp::View& view) {
  std::ostringstream stream;
  stream << "DidChangeView:"
         << " x:" << view.GetRect().x()
         << " y:" << view.GetRect().y()
         << " width:" << view.GetRect().width()
         << " height:" << view.GetRect().height()
         << "\n"
         << " IsFullscreen:" << view.IsFullscreen()
         << " IsVisible:" << view.IsVisible()
         << " IsPageVisible:" << view.IsPageVisible()
         << " GetDeviceScale:" << view.GetDeviceScale()
         << " GetCSSScale:" << view.GetCSSScale();
    PostMessage(stream.str());
  }

  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
      case PP_INPUTEVENT_TYPE_IME_TEXT:
      case PP_INPUTEVENT_TYPE_UNDEFINED:
        // these cases are not handled.
        break;
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      case PP_INPUTEVENT_TYPE_MOUSEUP:
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
      case PP_INPUTEVENT_TYPE_MOUSEENTER:
      case PP_INPUTEVENT_TYPE_MOUSELEAVE:
      case PP_INPUTEVENT_TYPE_CONTEXTMENU: {
        pp::MouseInputEvent mouse_event(event);
        std::ostringstream stream;
        stream << "Mouse event:"
               << " modifier:" << ModifierToString(mouse_event.GetModifiers())
               << " button:" << MouseButtonToString(mouse_event.GetButton())
               << " x:" << mouse_event.GetPosition().x()
               << " y:" << mouse_event.GetPosition().y()
               << " click_count:" << mouse_event.GetClickCount()
               << " time:" << mouse_event.GetTimeStamp()
               << " is_context_menu: "
                   << (event.GetType() == PP_INPUTEVENT_TYPE_CONTEXTMENU);
        PostMessage(stream.str());
        break;
      }

      case PP_INPUTEVENT_TYPE_WHEEL: {
        pp::WheelInputEvent wheel_event(event);
        std::ostringstream stream;
        stream << "Wheel event:"
               << " modifier:" << ModifierToString(wheel_event.GetModifiers())
               << " deltax:" << wheel_event.GetDelta().x()
               << " deltay:" << wheel_event.GetDelta().y()
               << " wheel_ticks_x:" << wheel_event.GetTicks().x()
               << " wheel_ticks_y:" << wheel_event.GetTicks().y()
               << " scroll_by_page: " << wheel_event.GetScrollByPage()
               << " time:" << wheel_event.GetTimeStamp();
        PostMessage(stream.str());
        break;
      }

      case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
      case PP_INPUTEVENT_TYPE_KEYDOWN:
      case PP_INPUTEVENT_TYPE_KEYUP:
      case PP_INPUTEVENT_TYPE_CHAR: {
        pp::KeyboardInputEvent key_event(event);
        std::ostringstream stream;
        stream << "Key event:"
               << " modifier:" << ModifierToString(key_event.GetModifiers())
               << " key_code:" << key_event.GetKeyCode()
               << " time:" << key_event.GetTimeStamp()
               << " text:" << key_event.GetCharacterText().DebugString();
        PostMessage(stream.str());
        break;
      }

      case PP_INPUTEVENT_TYPE_TOUCHSTART:
      case PP_INPUTEVENT_TYPE_TOUCHMOVE:
      case PP_INPUTEVENT_TYPE_TOUCHEND:
      case PP_INPUTEVENT_TYPE_TOUCHCANCEL: {
        pp::TouchInputEvent touch_event(event);
        std::ostringstream stream;
        stream << "Touch event:" << TouchKindToString(event.GetType())
               << " modifier:" << ModifierToString(touch_event.GetModifiers());

        uint32_t touch_count =
            touch_event.GetTouchCount(PP_TOUCHLIST_TYPE_CHANGEDTOUCHES);
        for (uint32_t i = 0; i < touch_count; ++i) {
          pp::TouchPoint point =
              touch_event.GetTouchByIndex(PP_TOUCHLIST_TYPE_CHANGEDTOUCHES, i);
          stream << " x[" << point.id() << "]:" << point.position().x()
                 << " y[" << point.id() << "]:" << point.position().y()
                 << " radii_x[" << point.id() << "]:" << point.radii().x()
                 << " radii_y[" << point.id() << "]:" << point.radii().y()
                 << " angle[" << point.id() << "]:" << point.rotation_angle()
                 << " pressure[" << point.id() << "]:" << point.pressure();
        }
        stream << " time:" << touch_event.GetTimeStamp();
        PostMessage(stream.str());
        break;
      }

      default: {
        // For any unhandled events, send a message to the browser
        // so that the user is aware of these and can investigate.
        std::stringstream oss;
        oss << "Default (unhandled) event, type=" << event.GetType();
        PostMessage(oss.str());
      } break;
    }

    return true;
  }
};

class InputEventModule : public pp::Module {
 public:
  InputEventModule() : pp::Module() {}
  virtual ~InputEventModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new InputEventInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new InputEventModule(); }
}

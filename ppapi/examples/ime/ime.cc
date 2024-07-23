// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/ppb_console.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/text_input_controller.h"
#include "ppapi/cpp/trusted/browser_font_trusted.h"

namespace {

// Extracted from: ui/events/keycodes/keyboard_codes.h
enum {
  VKEY_BACK = 0x08,
  VKEY_SHIFT = 0x10,
  VKEY_DELETE = 0x2E,
  VKEY_LEFT = 0x25,
  VKEY_UP = 0x26,
  VKEY_RIGHT = 0x27,
  VKEY_DOWN = 0x28,
};

const uint32_t kTextfieldBgColor = 0xffffffff;
const uint32_t kTextfieldTextColor = 0xff000000;
const uint32_t kTextfieldCaretColor = 0xff000000;
const uint32_t kTextfieldPreeditTextColor = 0xffff0000;
const uint32_t kTextfieldSelectionBackgroundColor = 0xffeecccc;
const uint32_t kTextfieldUnderlineColorMain = 0xffff0000;
const uint32_t kTextfieldUnderlineColorSub = 0xffddaaaa;

void FillRect(pp::ImageData* image,
              int left, int top, int width, int height,
              uint32_t color) {
  for (int y = std::max(0, top);
       y < std::min(image->size().height() - 1, top + height);
       ++y) {
    for (int x = std::max(0, left);
         x < std::min(image->size().width() - 1, left + width);
         ++x)
      *image->GetAddr32(pp::Point(x, y)) = color;
  }
}

void FillRect(pp::ImageData* image, const pp::Rect& rect, uint32_t color) {
  FillRect(image, rect.x(), rect.y(), rect.width(), rect.height(), color);
}

size_t GetPrevCharOffsetUtf8(const std::string& str, size_t current_pos) {
  size_t i = current_pos;
  if (i > 0) {
    do {
      --i;
    } while (i > 0 && (str[i] & 0xc0) == 0x80);
  }
  return i;
}

size_t GetNextCharOffsetUtf8(const std::string& str, size_t current_pos) {
  size_t i = current_pos;
  if (i < str.size()) {
    do {
      ++i;
    } while (i < str.size() && (str[i] & 0xc0) == 0x80);
  }
  return i;
}

size_t GetNthCharOffsetUtf8(const std::string& str, size_t n) {
  size_t i = 0;
  for (size_t step = 0; step < n; ++step)
    i = GetNextCharOffsetUtf8(str, i);
  return i;
}

}  // namespace

class TextFieldStatusHandler {
 public:
  virtual ~TextFieldStatusHandler() {}
  virtual void FocusIn(const pp::Rect& caret) {}
  virtual void FocusOut() {}
  virtual void UpdateSelection(const std::string& text) {}
};

class TextFieldStatusNotifyingHandler : public TextFieldStatusHandler {
 public:
  explicit TextFieldStatusNotifyingHandler(pp::Instance* instance)
      : textinput_control_(instance) {
  }

 protected:
  // Implement TextFieldStatusHandler.
  virtual void FocusIn(const pp::Rect& caret) {
    textinput_control_.SetTextInputType(PP_TEXTINPUT_TYPE_TEXT);
    textinput_control_.UpdateCaretPosition(caret);
  }
  virtual void FocusOut() {
    textinput_control_.CancelCompositionText();
    textinput_control_.SetTextInputType(PP_TEXTINPUT_TYPE_NONE);
  }
  virtual void UpdateSelection(const std::string& text) {
    textinput_control_.UpdateSurroundingText(
        text, 0, static_cast<uint32_t>(text.size()));
  }

 private:
  pp::TextInputController textinput_control_;
};

// Hand-made text field for demonstrating text input API.
class MyTextField {
 public:
  MyTextField(pp::Instance* instance, TextFieldStatusHandler* handler,
              int x, int y, int width, int height)
      : instance_(instance),
        status_handler_(handler),
        area_(x, y, width, height),
        font_size_(height - 2),
        caret_pos_(std::string::npos),
        anchor_pos_(std::string::npos),
        target_segment_(0) {
    pp::BrowserFontDescription desc;
    desc.set_family(PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF);
    desc.set_size(font_size_);
    font_ = pp::BrowserFont_Trusted(instance_, desc);
  }

  // Paint on the specified ImageData.
  void PaintOn(pp::ImageData* image, pp::Rect clip) {
    clip = clip.Intersect(area_);
    FillRect(image, clip, kTextfieldBgColor);

    if (caret_pos_ != std::string::npos) {
      int offset = area_.x();
      // selection (for the case without composition text)
      if (composition_.empty() && HasSelection()) {
        int left_x = font_.MeasureSimpleText(
            utf8_text_.substr(0, SelectionLeft()));
        int right_x = font_.MeasureSimpleText(
            utf8_text_.substr(0, SelectionRight()));
        FillRect(image, offset + left_x, area_.y(), right_x - left_x,
                 area_.height(), kTextfieldSelectionBackgroundColor);
      }
      // before caret
      {
        std::string str = utf8_text_.substr(0, caret_pos_);
        font_.DrawTextAt(
            image,
            pp::BrowserFontTextRun(str.c_str(), false, false),
            pp::Point(offset, area_.y() + font_size_),
            kTextfieldTextColor,
            clip,
            false);
        offset += font_.MeasureSimpleText(str);
      }
      // composition
      {
        const std::string& str = composition_;
        // selection
        if (composition_selection_.first != composition_selection_.second) {
          int left_x = font_.MeasureSimpleText(
              str.substr(0, composition_selection_.first));
          int right_x = font_.MeasureSimpleText(
              str.substr(0, composition_selection_.second));
          FillRect(image, offset + left_x, area_.y(), right_x - left_x,
                   area_.height(), kTextfieldSelectionBackgroundColor);
        }
        // composition text
        font_.DrawTextAt(
            image,
            pp::BrowserFontTextRun(str.c_str(), false, false),
            pp::Point(offset, area_.y() + font_size_),
            kTextfieldPreeditTextColor,
            clip,
            false);
        for (size_t i = 0; i < segments_.size(); ++i) {
          size_t l = segments_[i].first;
          size_t r = segments_[i].second;
          if (l != r) {
            int lx = font_.MeasureSimpleText(str.substr(0, l));
            int rx = font_.MeasureSimpleText(str.substr(0, r));
            FillRect(image,
                     offset + lx + 2, area_.y() + font_size_ + 1,
                     rx - lx - 4, 2,
                     i == static_cast<size_t>(target_segment_) ?
                         kTextfieldUnderlineColorMain :
                         kTextfieldUnderlineColorSub);
          }
        }
        // caret
        int caretx = font_.MeasureSimpleText(
            str.substr(0, composition_selection_.first));
        FillRect(image,
                 pp::Rect(offset + caretx, area_.y(), 2, area_.height()),
                 kTextfieldCaretColor);
        offset += font_.MeasureSimpleText(str);
      }
      // after caret
      {
        std::string str = utf8_text_.substr(caret_pos_);
        font_.DrawTextAt(
            image,
            pp::BrowserFontTextRun(str.c_str(), false, false),
            pp::Point(offset, area_.y() + font_size_),
            kTextfieldTextColor,
            clip,
            false);
      }
    } else {
      font_.DrawTextAt(
          image,
          pp::BrowserFontTextRun(utf8_text_.c_str(), false, false),
          pp::Point(area_.x(), area_.y() + font_size_),
          kTextfieldTextColor,
          clip,
          false);
    }
  }

  // Update current composition text.
  void SetComposition(
      const std::string& text,
      const std::vector< std::pair<uint32_t, uint32_t> >& segments,
      int32_t target_segment,
      const std::pair<uint32_t, uint32_t>& selection) {
    if (HasSelection() && !text.empty())
      InsertText(std::string());
    composition_ = text;
    segments_ = segments;
    target_segment_ = target_segment;
    composition_selection_ = selection;
    CaretPosChanged();
  }

  // Is the text field focused?
  bool Focused() const {
    return caret_pos_ != std::string::npos;
  }

  // Does the coordinate (x,y) is contained inside the edit box?
  bool Contains(int x, int y) const {
    return area_.Contains(x, y);
  }

  // Resets the content text.
  void SetText(const std::string& text) {
    utf8_text_ = text;
    if (Focused()) {
      caret_pos_ = anchor_pos_ = text.size();
      CaretPosChanged();
    }
  }

  // Inserts a text at the current caret position.
  void InsertText(const std::string& text) {
    if (!Focused())
      return;
    utf8_text_.replace(SelectionLeft(), SelectionRight() - SelectionLeft(),
                       text);
    caret_pos_ = anchor_pos_ = SelectionLeft() + text.size();
    CaretPosChanged();
  }

  // Handles mouse click event and changes the focus state.
  bool RefocusByMouseClick(int x, int y) {
    if (!Contains(x, y)) {
      // The text field is unfocused.
      caret_pos_ = anchor_pos_ = std::string::npos;
      return false;
    }

    // The text field is focused.
    size_t n = font_.CharacterOffsetForPixel(
        pp::BrowserFontTextRun(utf8_text_.c_str()), x - area_.x());
    caret_pos_ = anchor_pos_ = GetNthCharOffsetUtf8(utf8_text_, n);
    CaretPosChanged();
    return true;
  }

  void MouseDrag(int x, int y) {
    if (!Focused())
      return;
    size_t n = font_.CharacterOffsetForPixel(
        pp::BrowserFontTextRun(utf8_text_.c_str()), x - area_.x());
    caret_pos_ = GetNthCharOffsetUtf8(utf8_text_, n);
  }

  void MouseUp(int x, int y) {
    if (!Focused())
      return;
    CaretPosChanged();
  }

  void KeyLeft(bool shift) {
    if (!Focused())
      return;
    // Move caret to the head of the selection or to the previous character.
    if (!shift && HasSelection())
      caret_pos_ = SelectionLeft();
    else
      caret_pos_ = GetPrevCharOffsetUtf8(utf8_text_, caret_pos_);
    // Move the anchor if the shift key is not pressed.
    if (!shift)
      anchor_pos_ = caret_pos_;
    CaretPosChanged();
  }

  void KeyRight(bool shift) {
    if (!Focused())
      return;
    // Move caret to the end of the selection or to the next character.
    if (!shift && HasSelection())
      caret_pos_ = SelectionRight();
    else
      caret_pos_ = GetNextCharOffsetUtf8(utf8_text_, caret_pos_);
    // Move the anchor if the shift key is not pressed.
    if (!shift)
      anchor_pos_ = caret_pos_;
    CaretPosChanged();
  }

  void KeyDelete() {
    if (!Focused())
      return;
    if (HasSelection()) {
      InsertText(std::string());
    } else {
      size_t i = GetNextCharOffsetUtf8(utf8_text_, caret_pos_);
      utf8_text_.erase(caret_pos_, i - caret_pos_);
      CaretPosChanged();
    }
  }

  void KeyBackspace() {
    if (!Focused())
      return;
    if (HasSelection()) {
      InsertText(std::string());
    } else if (caret_pos_ != 0) {
      size_t i = GetPrevCharOffsetUtf8(utf8_text_, caret_pos_);
      utf8_text_.erase(i, caret_pos_ - i);
      caret_pos_ = anchor_pos_ = i;
      CaretPosChanged();
    }
  }

 private:
  // Notify the plugin instance that the caret position has changed.
  void CaretPosChanged() {
    if (Focused()) {
      std::string str = utf8_text_.substr(0, caret_pos_);
      if (!composition_.empty())
        str += composition_.substr(0, composition_selection_.first);
      int px = font_.MeasureSimpleText(str);
      pp::Rect caret(area_.x() + px, area_.y(), 0, area_.height() + 2);
      status_handler_->FocusIn(caret);
      status_handler_->UpdateSelection(
          utf8_text_.substr(SelectionLeft(),
                            SelectionRight() - SelectionLeft()));
    }
  }
  size_t SelectionLeft() const {
    return std::min(caret_pos_, anchor_pos_);
  }
  size_t SelectionRight() const {
    return std::max(caret_pos_, anchor_pos_);
  }
  bool HasSelection() const {
    return caret_pos_ != anchor_pos_;
  }

  pp::Instance* instance_;
  TextFieldStatusHandler* status_handler_;

  pp::Rect area_;
  int font_size_;
  pp::BrowserFont_Trusted font_;
  std::string utf8_text_;
  size_t caret_pos_;
  size_t anchor_pos_;
  std::string composition_;
  std::vector< std::pair<uint32_t, uint32_t> > segments_;
  std::pair<uint32_t, uint32_t> composition_selection_;
  int target_segment_;
};

class MyInstance : public pp::Instance {
 public:
  explicit MyInstance(PP_Instance instance)
      : pp::Instance(instance),
        status_handler_(new TextFieldStatusHandler),
        dragging_(false) {
  }

  ~MyInstance() {
    delete status_handler_;
  }

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
    RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);

    for (uint32_t i = 0; i < argc; ++i) {
      if (argn[i] == std::string("ime")) {
        if (argv[i] == std::string("no")) {
          // Example of NO-IME plugins (e.g., games).
          //
          // When a plugin never wants to accept text input, at initialization
          // explicitly turn off the text input feature by calling:
          pp::TextInputController(this).SetTextInputType(
              PP_TEXTINPUT_TYPE_NONE);
        } else if (argv[i] == std::string("unaware")) {
          // Demonstrating the behavior of IME-unaware plugins.
          // Never call any text input related APIs.
          //
          // In such a case, the plugin is assumed to always accept text input.
          // For example, when the plugin is focused in touch devices a virtual
          // keyboard may pop up, or in environment IME is used, users can type
          // text via IME on the plugin. The characters are delivered to the
          // plugin via PP_INPUTEVENT_TYPE_CHAR events.
        } else if (argv[i] == std::string("caretmove")) {
          // Demonstrating the behavior of plugins with limited IME support.
          //
          // It uses SetTextInputType() and UpdateCaretPosition() API to notify
          // text input status to the browser, but unable to handle inline
          // compositions. By using the notified information. the browser can,
          // say, show virtual keyboards or IMEs only at appropriate timing
          // that the plugin does need to accept text input.
          delete status_handler_;
          status_handler_ = new TextFieldStatusNotifyingHandler(this);
        } else if (argv[i] == std::string("full")) {
          // Demonstrating the behavior of plugins fully supporting IME.
          //
          // It notifies updates of caret positions to the browser,
          // and handles all text input events by itself.
          delete status_handler_;
          status_handler_ = new TextFieldStatusNotifyingHandler(this);
          RequestInputEvents(PP_INPUTEVENT_CLASS_IME);
        }
        break;
      }
    }

    textfield_.push_back(MyTextField(this, status_handler_,
                                     10, 10, 300, 20));
    textfield_.back().SetText("Hello");
    textfield_.push_back(MyTextField(this, status_handler_,
                                     30, 100, 300, 20));
    textfield_.back().SetText("World");
    return true;
  }

 protected:
  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    bool ret = false;
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_MOUSEDOWN: {
        const pp::MouseInputEvent mouseEvent(event);
        ret = OnMouseDown(mouseEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_MOUSEMOVE: {
        const pp::MouseInputEvent mouseEvent(event);
        ret = OnMouseMove(mouseEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_MOUSEUP: {
        const pp::MouseInputEvent mouseEvent(event);
        ret = OnMouseUp(mouseEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_KEYDOWN: {
        Log("Keydown");
        const pp::KeyboardInputEvent keyEvent(event);
        ret = OnKeyDown(keyEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_CHAR: {
        const pp::KeyboardInputEvent keyEvent(event);
        Log("Char [" + keyEvent.GetCharacterText().AsString() + "]");
        ret = OnChar(keyEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START: {
        const pp::IMEInputEvent imeEvent(event);
        Log("CompositionStart [" + imeEvent.GetText().AsString() + "]");
        ret = true;
        break;
      }
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE: {
        const pp::IMEInputEvent imeEvent(event);
        Log("CompositionUpdate [" + imeEvent.GetText().AsString() + "]");
        ret = OnCompositionUpdate(imeEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END: {
        const pp::IMEInputEvent imeEvent(event);
        Log("CompositionEnd [" + imeEvent.GetText().AsString() + "]");
        ret = OnCompositionEnd(imeEvent);
        break;
      }
      case PP_INPUTEVENT_TYPE_IME_TEXT: {
        const pp::IMEInputEvent imeEvent(event);
        Log("ImeText [" + imeEvent.GetText().AsString() + "]");
        ret = OnImeText(imeEvent);
        break;
      }
      default:
        break;
    }
    if (ret && (dragging_ || event.GetType() != PP_INPUTEVENT_TYPE_MOUSEMOVE))
      Paint();
    return ret;
  }

  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
    if (position.size() == last_size_)
      return;
    last_size_ = position.size();
    Paint();
  }

 private:
  bool OnCompositionUpdate(const pp::IMEInputEvent& ev) {
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Focused()) {
        std::vector< std::pair<uint32_t, uint32_t> > segs;
        for (uint32_t i = 0; i < ev.GetSegmentNumber(); ++i)
          segs.push_back(std::make_pair(ev.GetSegmentOffset(i),
                                        ev.GetSegmentOffset(i + 1)));
        uint32_t selection_start;
        uint32_t selection_end;
        ev.GetSelection(&selection_start, &selection_end);
        it->SetComposition(ev.GetText().AsString(),
                           segs,
                           ev.GetTargetSegment(),
                           std::make_pair(selection_start, selection_end));
        return true;
      }
    }
    return false;
  }

  bool OnCompositionEnd(const pp::IMEInputEvent& ev) {
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Focused()) {
        it->SetComposition(std::string(),
                           std::vector<std::pair<uint32_t, uint32_t> >(),
                           0,
                           std::make_pair(0, 0));
        return true;
      }
    }
    return false;
  }

  bool OnMouseDown(const pp::MouseInputEvent& ev) {
    dragging_ = true;

    bool anyone_focused = false;
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->RefocusByMouseClick(ev.GetPosition().x(),
                                  ev.GetPosition().y())) {
        anyone_focused = true;
      }
    }
    if (!anyone_focused)
      status_handler_->FocusOut();
    return true;
  }

  bool OnMouseMove(const pp::MouseInputEvent& ev) {
    const PPB_CursorControl_Dev* cursor_control =
        reinterpret_cast<const PPB_CursorControl_Dev*>(
            pp::Module::Get()->GetBrowserInterface(
                PPB_CURSOR_CONTROL_DEV_INTERFACE));
    if (!cursor_control)
      return false;

    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Contains(ev.GetPosition().x(),
                       ev.GetPosition().y())) {
        cursor_control->SetCursor(pp_instance(), PP_CURSORTYPE_IBEAM,
                                  0, NULL);
        if (it->Focused() && dragging_)
          it->MouseDrag(ev.GetPosition().x(), ev.GetPosition().y());
        return true;
      }
    }
    cursor_control->SetCursor(pp_instance(), PP_CURSORTYPE_POINTER,
                              0, NULL);
    return true;
  }

  bool OnMouseUp(const pp::MouseInputEvent& ev) {
    dragging_ = false;
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it)
      if (it->Focused())
        it->MouseUp(ev.GetPosition().x(), ev.GetPosition().y());
    return false;
  }

  bool OnKeyDown(const pp::KeyboardInputEvent& ev) {
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Focused()) {
        bool shift = ev.GetModifiers() & PP_INPUTEVENT_MODIFIER_SHIFTKEY;
        switch (ev.GetKeyCode()) {
          case VKEY_LEFT:
            it->KeyLeft(shift);
            break;
          case VKEY_RIGHT:
            it->KeyRight(shift);
            break;
          case VKEY_DELETE:
            it->KeyDelete();
            break;
          case VKEY_BACK:
            it->KeyBackspace();
            break;
        }
        return true;
      }
    }
    return false;
  }

  bool OnChar(const pp::KeyboardInputEvent& ev) {
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Focused()) {
        std::string str = ev.GetCharacterText().AsString();
        if (str != "\r" && str != "\n")
          it->InsertText(str);
        return true;
      }
    }
    return false;
  }

  bool OnImeText(const pp::IMEInputEvent ev) {
    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      if (it->Focused()) {
        it->InsertText(ev.GetText().AsString());
        return true;
      }
    }
    return false;
  }

  void Paint() {
    pp::Rect clip(0, 0, last_size_.width(), last_size_.height());
    PaintClip(clip);
  }

  void PaintClip(const pp::Rect& clip) {
    pp::ImageData image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL, last_size_, true);
    pp::Graphics2D device(this, last_size_, false);
    BindGraphics(device);

    for (std::vector<MyTextField>::iterator it = textfield_.begin();
         it != textfield_.end();
         ++it) {
      it->PaintOn(&image, clip);
    }

    device.PaintImageData(image, pp::Point(0, 0));
    device.Flush(pp::CompletionCallback(&OnFlush, this));
  }

  static void OnFlush(void* user_data, int32_t result) {}

  // Prints a debug message.
  void Log(const pp::Var& value) {
    const PPB_Console* console = reinterpret_cast<const PPB_Console*>(
        pp::Module::Get()->GetBrowserInterface(PPB_CONSOLE_INTERFACE));
    if (!console)
      return;
    console->Log(pp_instance(), PP_LOGLEVEL_LOG, value.pp_var());
  }

  // IME Control interface.
  TextFieldStatusHandler* status_handler_;

  // Remembers the size of this instance.
  pp::Size last_size_;

  // Holds instances of text fields.
  std::vector<MyTextField> textfield_;

  // Whether or not during a drag operation.
  bool dragging_;
};

class MyModule : public pp::Module {
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new MyInstance(instance);
  }
};

namespace pp {

Module* CreateModule() {
  return new MyModule();
}

}  // namespace pp

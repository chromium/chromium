// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_INPUT_EVENT_H_
#define PPAPI_CPP_INPUT_EVENT_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "ppapi/c/ppb_input_event.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/touch_point.h"

/// @file
/// This file defines the API used to handle mouse and keyboard input events.

namespace pp {

class FloatPoint;
class InstanceHandle;
class Point;
class Var;

/// This class represents an input event resource. Normally you will get passed
/// this object through the HandleInputEvent() function on the
/// <code>Instance</code> object.
///
/// Typically you would check the type of the event and then create the
/// appropriate event-specific object to query the properties.
///
/// <strong>Example:</strong>
/// @code
///
/// bool MyInstance::HandleInputEvent(const pp::InputEvent& event) {
///   switch (event.GetType()) {
///     case PP_INPUTEVENT_TYPE_MOUSEDOWN {
///       pp::MouseInputEvent mouse_event(event);
///       return HandleMouseDown(mouse_event.GetMousePosition());
///     }
///     default:
///       return false;
/// }
///
/// @endcode
class InputEvent : public Resource {
 public:
  /// Default constructor that creates an is_null() InputEvent object.
  InputEvent();

  /// This constructor constructs an input event from the provided input event
  /// resource ID. The InputEvent object will be is_null() if the given
  /// resource is not a valid input event.
  ///
  /// @param[in] input_event_resource A input event resource ID.
  explicit InputEvent(PP_Resource input_event_resource);

  ~InputEvent();

  /// GetType() returns the type of input event for this input event
  /// object.
  ///
  /// @return A <code>PP_InputEvent_Type</code> if successful,
  /// PP_INPUTEVENT_TYPE_UNDEFINED if the resource is invalid.
  PP_InputEvent_Type GetType() const;

  /// GetTimeStamp() returns the time that the event was generated. The time
  /// will be before the current time since processing and dispatching the
  /// event has some overhead. Use this value to compare the times the user
  /// generated two events without being sensitive to variable processing time.
  ///
  /// The return value is in time ticks, which is a monotonically increasing
  /// clock not related to the wall clock time. It will not change if the user
  /// changes their clock or daylight savings time starts, so can be reliably
  /// used to compare events. This means, however, that you can't correlate
  /// event times to a particular time of day on the system clock.
  ///
  /// @return A <code>PP_TimeTicks</code> containing the time the event was
  /// generated.
  PP_TimeTicks GetTimeStamp() const;

  /// GetModifiers() returns a bitfield indicating which modifiers were down
  /// at the time of the event. This is a combination of the flags in the
  /// <code>PP_InputEvent_Modifier</code> enum.
  ///
  /// @return The modifiers associated with the event, or 0 if the given
  /// resource is not a valid event resource.
  uint32_t GetModifiers() const;
};

/// This class handles mouse events.
class MouseInputEvent : public InputEvent {
 public:
  /// Constructs an is_null() mouse input event object.
  MouseInputEvent();

  /// This constructor constructs a mouse input event object from the provided
  /// generic input event. If the given event is itself is_null() or is not
  /// a mouse input event, the mouse object will be is_null().
  ///
  /// @param event An <code>InputEvent</code>.
  explicit MouseInputEvent(const InputEvent& event);

  /// This constructor manually constructs a mouse event from the provided
  /// parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
  /// input event.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in] modifiers A bit field combination of the
  /// <code>PP_InputEvent_Modifier</code> flags.
  ///
  /// @param[in] mouse_button The button that changed for mouse down or up
  /// events. This value will be <code>PP_EVENT_MOUSEBUTTON_NONE</code> for
  /// mouse move, enter, and leave events.
  ///
  /// @param[in] mouse_position A <code>Point</code> containing the x and y
  /// position of the mouse when the event occurred.
  ///
  /// @param[in] click_count
  // TODO(brettw) figure out exactly what this means.
  ///
  /// @param[in] mouse_movement The change in position of the mouse.
  MouseInputEvent(const InstanceHandle& instance,
                  PP_InputEvent_Type type,
                  PP_TimeTicks time_stamp,
                  uint32_t modifiers,
                  PP_InputEvent_MouseButton mouse_button,
                  const Point& mouse_position,
                  int32_t click_count,
                  const Point& mouse_movement);

  /// GetButton() returns the mouse position for a mouse input event.
  ///
  /// @return The mouse button associated with mouse down and up events. This
  /// value will be PP_EVENT_MOUSEBUTTON_NONE for mouse move, enter, and leave
  /// events, and for all non-mouse events.
  PP_InputEvent_MouseButton GetButton() const;

  /// GetPosition() returns the pixel location of a mouse input event. When
  /// the mouse is locked, it returns the last known mouse position just as
  /// mouse lock was entered.
  ///
  /// @return The point associated with the mouse event, relative to the upper-
  /// left of the instance receiving the event. These values can be negative for
  /// mouse drags. The return value will be (0, 0) for non-mouse events.
  Point GetPosition() const;

  // TODO(brettw) figure out exactly what this means.
  int32_t GetClickCount() const;

  /// Returns the change in position of the mouse. When the mouse is locked,
  /// although the mouse position doesn't actually change, this function
  /// still provides movement information, which indicates what the change in
  /// position would be had the mouse not been locked.
  ///
  /// @return The change in position of the mouse, relative to the previous
  /// position.
  Point GetMovement() const;
};

class WheelInputEvent : public InputEvent {
 public:
  /// Constructs an is_null() wheel input event object.
  WheelInputEvent();

  /// This constructor constructs a wheel input event object from the
  /// provided generic input event. If the given event is itself
  /// is_null() or is not a wheel input event, the wheel object will be
  /// is_null().
  ///
  /// @param[in] event A generic input event.
  explicit WheelInputEvent(const InputEvent& event);

  /// Constructs a wheel input even from the given parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in] modifiers A bit field combination of the
  /// <code>PP_InputEvent_Modifier</code> flags.
  ///
  /// @param[in] wheel_delta The scroll wheel's horizontal and vertical scroll
  /// amounts.
  ///
  /// @param[in] wheel_ticks The number of "clicks" of the scroll wheel that
  /// have produced the event.
  ///
  /// @param[in] scroll_by_page When true, the user is requesting to scroll
  /// by pages. When false, the user is requesting to scroll by lines.
  WheelInputEvent(const InstanceHandle& instance,
                  PP_TimeTicks time_stamp,
                  uint32_t modifiers,
                  const FloatPoint& wheel_delta,
                  const FloatPoint& wheel_ticks,
                  bool scroll_by_page);

  /// GetDelta() returns the amount vertically and horizontally the user has
  /// requested to scroll by with their mouse wheel. A scroll down or to the
  /// right (where the content moves up or left) is represented as positive
  /// values, and a scroll up or to the left (where the content moves down or
  /// right) is represented as negative values.
  ///
  /// This amount is system dependent and will take into account the user's
  /// preferred scroll sensitivity and potentially also nonlinear acceleration
  /// based on the speed of the scrolling.
  ///
  /// Devices will be of varying resolution. Some mice with large detents will
  /// only generate integer scroll amounts. But fractional values are also
  /// possible, for example, on some trackpads and newer mice that don't have
  /// "clicks".
  ///
  /// @return The vertical and horizontal scroll values. The units are either in
  /// pixels (when scroll_by_page is false) or pages (when scroll_by_page is
  /// true). For example, y = -3 means scroll up 3 pixels when scroll_by_page
  /// is false, and scroll up 3 pages when scroll_by_page is true.
  FloatPoint GetDelta() const;

  /// GetTicks() returns the number of "clicks" of the scroll wheel
  /// that have produced the event. The value may have system-specific
  /// acceleration applied to it, depending on the device. The positive and
  /// negative meanings are the same as for GetDelta().
  ///
  /// If you are scrolling, you probably want to use the delta values.  These
  /// tick events can be useful if you aren't doing actual scrolling and don't
  /// want or pixel values. An example may be cycling between different items in
  /// a game.
  ///
  /// @return The number of "clicks" of the scroll wheel. You may receive
  /// fractional values for the wheel ticks if the mouse wheel is high
  /// resolution or doesn't have "clicks". If your program wants discrete
  /// events (as in the "picking items" example) you should accumulate
  /// fractional click values from multiple messages until the total value
  /// reaches positive or negative one. This should represent a similar amount
  /// of scrolling as for a mouse that has a discrete mouse wheel.
  FloatPoint GetTicks() const;

  /// GetScrollByPage() indicates if the scroll delta x/y indicates pages or
  /// lines to scroll by.
  ///
  /// @return true if the event is a wheel event and the user is scrolling
  /// by pages, false if not or if the resource is not a wheel event.
  bool GetScrollByPage() const;
};

class KeyboardInputEvent : public InputEvent {
 public:
  /// Constructs an is_null() keyboard input event object.
  KeyboardInputEvent();

  /// Constructs a keyboard input event object from the provided generic input
  /// event. If the given event is itself is_null() or is not a keyboard input
  /// event, the keybaord object will be is_null().
  ///
  /// @param[in] event A generic input event.
  explicit KeyboardInputEvent(const InputEvent& event);

  /// Constructs a keyboard input even from the given parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
  /// input event.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in]  modifiers A bit field combination of the
  /// <code>PP_InputEvent_Modifier</code> flags.
  ///
  /// @param[in] key_code This value reflects the DOM KeyboardEvent
  /// <code>keyCode</code> field. Chrome populates this with the Windows-style
  /// Virtual Key code of the key.
  ///
  /// @param[in] character_text This value represents the typed character as a
  /// UTF-8 string.
  KeyboardInputEvent(const InstanceHandle& instance,
                     PP_InputEvent_Type type,
                     PP_TimeTicks time_stamp,
                     uint32_t modifiers,
                     uint32_t key_code,
                     const Var& character_text);

  /// Constructs a keyboard input even from the given parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
  /// input event.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in]  modifiers A bit field combination of the
  /// <code>PP_InputEvent_Modifier</code> flags.
  ///
  /// @param[in] key_code This value reflects the DOM KeyboardEvent
  /// <code>keyCode</code> field. Chrome populates this with the Windows-style
  /// Virtual Key code of the key.
  ///
  /// @param[in] character_text This value represents the typed character as a
  /// UTF-8 string.
  ///
  /// @param[in] code This value reflects the DOM KeyboardEvent
  /// <code>code</code> field, which identifies the physical key associated
  /// with the event.
  KeyboardInputEvent(const InstanceHandle& instance,
                     PP_InputEvent_Type type,
                     PP_TimeTicks time_stamp,
                     uint32_t modifiers,
                     uint32_t key_code,
                     const Var& character_text,
                     const Var& code);

  /// Returns the DOM keyCode field for the keyboard event.
  /// Chrome populates this with the Windows-style Virtual Key code of the key.
  uint32_t GetKeyCode() const;

  /// Returns the typed character for the given character event.
  ///
  /// @return A string var representing a single typed character for character
  /// input events. For non-character input events the return value will be an
  /// undefined var.
  Var GetCharacterText() const;

  /// Returns the DOM |code| for the keyboard event.
  //
  /// @return A string var representing a physical key that was pressed to
  /// generate this event.
  Var GetCode() const;
};

class TouchInputEvent : public InputEvent {
 public:
  /// Constructs an is_null() touch input event object.
  TouchInputEvent();

  /// Constructs a touch input event object from the given generic input event.
  /// If the given event is itself is_null() or is not a touch input event, the
  /// touch object will be is_null().
  explicit TouchInputEvent(const InputEvent& event);

  /// Constructs a touch input even from the given parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
  /// input event.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in]  modifiers A bit field combination of the
  /// <code>PP_InputEvent_Modifier</code> flags.
  TouchInputEvent(const InstanceHandle& instance,
                  PP_InputEvent_Type type,
                  PP_TimeTicks time_stamp,
                  uint32_t modifiers);

  /// Adds the touch-point to the specified TouchList.
  void AddTouchPoint(PP_TouchListType list, PP_TouchPoint point);

  /// @return The number of TouchPoints in this TouchList.
  uint32_t GetTouchCount(PP_TouchListType list) const;

  /// @return The TouchPoint at the given index of the given list, or an empty
  /// TouchPoint if the index is out of range.
  TouchPoint GetTouchByIndex(PP_TouchListType list, uint32_t index) const;

  /// @return The TouchPoint in the given list with the given identifier, or an
  /// empty TouchPoint if the list does not contain a TouchPoint with that
  /// identifier.
  TouchPoint GetTouchById(PP_TouchListType list, uint32_t id) const;
};

class IMEInputEvent : public InputEvent {
 public:
  /// Constructs an is_null() IME input event object.
  IMEInputEvent();

  /// Constructs an IME input event object from the provided generic input
  /// event. If the given event is itself is_null() or is not an IME input
  /// event, the object will be is_null().
  ///
  /// @param[in] event A generic input event.
  explicit IMEInputEvent(const InputEvent& event);

  /// This constructor manually constructs an IME event from the provided
  /// parameters.
  ///
  /// @param[in] instance The instance for which this event occurred.
  ///
  /// @param[in] type A <code>PP_InputEvent_Type</code> identifying the type of
  /// input event. The type must be one of the ime event types.
  ///
  /// @param[in] time_stamp A <code>PP_TimeTicks</code> indicating the time
  /// when the event occurred.
  ///
  /// @param[in] text The string returned by <code>GetText</code>.
  ///
  /// @param[in] segment_offsets The array of numbers returned by
  /// <code>GetSegmentOffset</code>.
  ///
  /// @param[in] target_segment The number returned by
  /// <code>GetTargetSegment</code>.
  ///
  /// @param[in] selection The range returned by <code>GetSelection</code>.
  IMEInputEvent(const InstanceHandle& instance,
                PP_InputEvent_Type type,
                PP_TimeTicks time_stamp,
                const Var& text,
                const std::vector<uint32_t>& segment_offsets,
                int32_t target_segment,
                const std::pair<uint32_t, uint32_t>& selection);

  /// Returns the composition text as a UTF-8 string for the given IME event.
  ///
  /// @return A string var representing the composition text. For non-IME
  /// input events the return value will be an undefined var.
  Var GetText() const;

  /// Returns the number of segments in the composition text.
  ///
  /// @return The number of segments. For events other than COMPOSITION_UPDATE,
  /// returns 0.
  uint32_t GetSegmentNumber() const;

  /// Returns the position of the index-th segmentation point in the composition
  /// text. The position is given by a byte-offset (not a character-offset) of
  /// the string returned by GetText(). It always satisfies
  /// 0=GetSegmentOffset(0) < ... < GetSegmentOffset(i) < GetSegmentOffset(i+1)
  /// < ... < GetSegmentOffset(GetSegmentNumber())=(byte-length of GetText()).
  /// Note that [GetSegmentOffset(i), GetSegmentOffset(i+1)) represents the
  /// range of the i-th segment, and hence GetSegmentNumber() can be a valid
  /// argument to this function instead of an off-by-1 error.
  ///
  /// @param[in] ime_event A <code>PP_Resource</code> corresponding to an IME
  /// event.
  ///
  /// @param[in] index An integer indicating a segment.
  ///
  /// @return The byte-offset of the segmentation point. If the event is not
  /// COMPOSITION_UPDATE or index is out of range, returns 0.
  uint32_t GetSegmentOffset(uint32_t index) const;

  /// Returns the index of the current target segment of composition.
  ///
  /// @return An integer indicating the index of the target segment. When there
  /// is no active target segment, or the event is not COMPOSITION_UPDATE,
  /// returns -1.
  int32_t GetTargetSegment() const;

  /// Obtains the range selected by caret in the composition text.
  ///
  /// @param[out] start An integer indicating a start offset of selection range.
  ///
  /// @param[out] end An integer indicating an end offset of selection range.
  void GetSelection(uint32_t* start, uint32_t* end) const;
};
}  // namespace pp

#endif  // PPAPI_CPP_INPUT_EVENT_H_

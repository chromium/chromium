// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary Bounds {
  // The x-coordinate of the upper-left corner.
  required long left;

  // The y-coordinate of the upper-left corner.
  required long top;

  // The width of the display in pixels.
  required long width;

  // The height of the display in pixels.
  required long height;
};

dictionary Insets {
  // The x-axis distance from the left bound.
  required long left;

  // The y-axis distance from the top bound.
  required long top;

  // The x-axis distance from the right bound.
  required long right;

  // The y-axis distance from the bottom bound.
  required long bottom;
};

dictionary Point {
  // The x-coordinate of the point.
  required long x;

  // The y-coordinate of the point.
  required long y;
};

dictionary TouchCalibrationPair {
  // The coordinates of the display point.
  required Point displayPoint;

  // The coordinates of the touch point corresponding to the display point.
  required Point touchPoint;
};

dictionary TouchCalibrationPairQuad {
  // First pair of touch and display point required for touch calibration.
  required TouchCalibrationPair pair1;

  // Second pair of touch and display point required for touch calibration.
  required TouchCalibrationPair pair2;

  // Third pair of touch and display point required for touch calibration.
  required TouchCalibrationPair pair3;

  // Fourth pair of touch and display point required for touch calibration.
  required TouchCalibrationPair pair4;
};

dictionary DisplayMode {
  // The display mode width in device independent (user visible) pixels.
  required long width;

  // The display mode height in device independent (user visible) pixels.
  required long height;

  // The display mode width in native pixels.
  required long widthInNativePixels;

  // The display mode height in native pixels.
  required long heightInNativePixels;

  // The display mode UI scale factor.
  [deprecated="Use $(ref: displayZoomFactor)"] double uiScale;

  // The display mode device scale factor.
  required double deviceScaleFactor;

  // The display mode refresh rate in hertz.
  required double refreshRate;

  // True if the mode is the display's native mode.
  required boolean isNative;

  // True if the display mode is currently selected.
  required boolean isSelected;

  // True if this mode is interlaced, false if not provided.
  boolean isInterlaced;
};

// Layout position, i.e. edge of parent that the display is attached to.
enum LayoutPosition {
  "top",
  "right",
  "bottom",
  "left"
};

dictionary DisplayLayout {
  // The unique identifier of the display.
  required DOMString id;

  // The unique identifier of the parent display. Empty if this is the root.
  required DOMString parentId;

  // The layout position of this display relative to the parent. This will
  // be ignored for the root.
  required LayoutPosition position;

  // The offset of the display along the connected edge. 0 indicates that
  // the topmost or leftmost corners are aligned.
  required long offset;
};

// EDID extracted parameters. Field description refers to "VESA ENHANCED
// EXTENDED DISPLAY IDENTIFICATION DATA STANDARD (Defines EDID Structure
// Version 1, Revision 4)" Release A, Revision 2 September 25, 2006.
// https://www.vesa.org/vesa-standards
dictionary Edid {
  // 3 character manufacturer code. See Sec. 3.4.1 page 21. Required in v1.4.
  required DOMString manufacturerId;

  // 2 byte manufacturer-assigned code, Sec. 3.4.2 page 21. Required in v1.4.
  required DOMString productId;

  // Year of manufacturer, Sec. 3.4.4 page 22. Required in v1.4.
  required long yearOfManufacture;
};

// An enum to tell if the display is detected and used by the
// system. The display is considered 'inactive', if it is not
// detected by the system (maybe disconnected, or considered
// disconnected due to sleep mode, etc). This state is used to keep
// existing display when the all displays are disconnected, for
// example.
enum ActiveState {
  "active",
  "inactive"
};

dictionary DisplayUnitInfo {
  // The unique identifier of the display.
  required DOMString id;

  // The user-friendly name (e.g. "HP LCD monitor").
  required DOMString name;

  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  Edid edid;

  // ChromeOS only. Identifier of the display that is being mirrored if
  // mirroring is enabled, otherwise empty. This will be set for all displays
  // (including the display being mirrored).
  required DOMString mirroringSourceId;

  // ChromeOS only. Identifiers of the displays to which the source display
  // is being mirrored. Empty if no displays are being mirrored. This will be
  // set to the same value for all displays. This must not include
  // |mirroringSourceId|.
  required sequence<DOMString> mirroringDestinationIds;

  // True if this is the primary display.
  required boolean isPrimary;

  // True if this is an internal display.
  required boolean isInternal;

  // True if this display is enabled.
  required boolean isEnabled;

  // Active if the display is detected and used by the system.
  required ActiveState activeState;

  // True for all displays when in unified desktop mode. See documentation
  // for $(ref:enableUnifiedDesktop).
  required boolean isUnified;

  // True when the auto-rotation is allowed. It happens when the device is in
  // a tablet physical state or kSupportsClamshellAutoRotation is set.
  // Provided for ChromeOS Settings UI only. TODO(stevenjb): Remove when
  // Settings switches to a mojo API.
  [nodoc] boolean isAutoRotationAllowed;

  // The number of pixels per inch along the x-axis.
  required double dpiX;

  // The number of pixels per inch along the y-axis.
  required double dpiY;

  // The display's clockwise rotation in degrees relative to the vertical
  // position.
  // Currently exposed only on ChromeOS. Will be set to 0 on other platforms.
  // A value of -1 will be interpreted as auto-rotate when the device is in
  // a physical tablet state.
  required long rotation;

  // The display's logical bounds.
  required Bounds bounds;

  // The display's insets within its screen's bounds.
  // Currently exposed only on ChromeOS. Will be set to empty insets on
  // other platforms.
  required Insets overscan;

  // The usable work area of the display within the display bounds. The work
  // area excludes areas of the display reserved for OS, for example taskbar
  // and launcher.
  required Bounds workArea;

  // The list of available display modes. The current mode will have
  // isSelected=true. Only available on ChromeOS. Will be set to an empty
  // array on other platforms.
  required sequence<DisplayMode> modes;

  // True if this display has a touch input device associated with it.
  required boolean hasTouchSupport;

  // True if this display has an accelerometer associated with it.
  // Provided for ChromeOS Settings UI only. TODO(stevenjb): Remove when
  // Settings switches to a mojo API. NOTE: The name of this may change.
  [nodoc] required boolean hasAccelerometerSupport;

  // A list of zoom factor values that can be set for the display.
  required sequence<double> availableDisplayZoomFactors;

  // The ratio between the display's current and default zoom.
  // For example, value 1 is equivalent to 100% zoom, and value 1.5 is
  // equivalent to 150% zoom.
  required double displayZoomFactor;
};

dictionary DisplayProperties {
  // ChromeOS only. If set to true, changes the display mode to unified
  // desktop (see $(ref:enableUnifiedDesktop) for details). If set to false,
  // unified desktop mode will be disabled. This is only valid for the
  // primary display. If provided, mirroringSourceId must not be provided and
  // other properties will be ignored. This is has no effect if not provided.
  boolean isUnified;

  // ChromeOS only. If set and not empty, enables mirroring for this display
  // only. Otherwise disables mirroring for all displays. This value should
  // indicate the id of the source display to mirror, which must not be the
  // same as the id passed to setDisplayProperties. If set, no other property
  // may be set.
  [deprecated="Use $(ref:setMirrorMode)."] DOMString mirroringSourceId;

  // If set to true, makes the display primary. No-op if set to false.
  // Note: If set, the display is considered primary for all other properties
  // (i.e. $(ref:isUnified) may be set and bounds origin may not).
  boolean isPrimary;

  // If set, sets the display's overscan insets to the provided values. Note
  // that overscan values may not be negative or larger than a half of the
  // screen's size. Overscan cannot be changed on the internal monitor.
  Insets overscan;

  // If set, updates the display's rotation.
  // Legal values are [0, 90, 180, 270]. The rotation is set clockwise,
  // relative to the display's vertical position.
  long rotation;

  // If set, updates the display's logical bounds origin along the x-axis.
  // Applied together with $(ref:boundsOriginY). Defaults to the current value
  // if not set and $(ref:boundsOriginY) is set. Note that when updating the
  // display origin, some constraints will be applied, so the final bounds
  // origin may be different than the one set. The final bounds can be
  // retrieved using $(ref:getInfo). The bounds origin cannot be changed on
  // the primary display.
  long boundsOriginX;

  // If set, updates the display's logical bounds origin along the y-axis.
  // See documentation for $(ref:boundsOriginX) parameter.
  long boundsOriginY;

  // If set, updates the display mode to the mode matching this value.
  // If other parameters are invalid, this will not be applied. If the
  // display mode is invalid, it will not be applied and an error will be
  // set, but other properties will still be applied.
  DisplayMode displayMode;

  // If set, updates the zoom associated with the display. This zoom performs
  // re-layout and repaint thus resulting in a better quality zoom than just
  // performing a pixel by pixel stretch enlargement.
  double displayZoomFactor;
};

dictionary GetInfoFlags {
  // If set to true, only a single $(ref:DisplayUnitInfo) will be returned
  // by $(ref:getInfo) when in unified desktop mode (see
  // $(ref:enableUnifiedDesktop)). Defaults to false.
  boolean singleUnified;
};

// Mirror mode, i.e. different ways of how a display is mirrored to other
// displays.
enum MirrorMode {
  // Specifies the default mode (extended or unified desktop).
  "off",

  // Specifies that the default source display will be mirrored to all other
  // displays.
  "normal",

  // Specifies that the specified source display will be mirrored to the
  // provided destination displays. All other connected displays will be
  //  extended.
  "mixed"
};

dictionary MirrorModeInfo {
  // The mirror mode that should be set.
  required MirrorMode mode;

  // The id of the mirroring source display. This is only valid for 'mixed'.
  DOMString mirroringSourceId;

  // The ids of the mirroring destination displays. This is only valid for
  // 'mixed'.
  sequence<DOMString> mirroringDestinationIds;
};

callback OnDisplayChangedListener = undefined ();

interface OnDisplayChangedEvent : ExtensionEvent {
  static undefined addListener(OnDisplayChangedListener listener);
  static undefined removeListener(OnDisplayChangedListener listener);
  static boolean hasListener(OnDisplayChangedListener listener);
};

// Use the <code>system.display</code> API to query display metadata.
interface Display {
  // Requests the information for all attached display devices.
  // |flags|: Options affecting how the information is returned.
  // |Returns|: The callback to invoke with the results.
  // |PromiseValue|: displayInfo
  [requiredCallback] static Promise<sequence<DisplayUnitInfo>> getInfo(
      optional GetInfoFlags flags);

  // Requests the layout info for all displays.
  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  // |Returns|: The callback to invoke with the results.
  // |PromiseValue|: layouts
  [requiredCallback] static Promise<sequence<DisplayLayout>> getDisplayLayout();

  // Updates the properties for the display specified by |id|, according to
  // the information provided in |info|. On failure, $(ref:runtime.lastError)
  // will be set.
  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  // |id|: The display's unique identifier.
  // |info|: The information about display properties that should be changed.
  //     A property will be changed only if a new value for it is specified in
  //     |info|.
  // |Returns|: Empty function called when the function finishes. To find out
  //     whether the function succeeded, $(ref:runtime.lastError) should be
  //     queried.
  static Promise<undefined> setDisplayProperties(
      DOMString id,
      DisplayProperties info);

  // Set the layout for all displays. Any display not included will use the
  // default layout. If a layout would overlap or be otherwise invalid it
  // will be adjusted to a valid layout. After layout is resolved, an
  // onDisplayChanged event will be triggered.
  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  // |layouts|: The layout information, required for all displays except
  //     the primary display.
  // |Returns|: Empty function called when the function finishes. To find out
  //     whether the function succeeded, $(ref:runtime.lastError) should be
  //     queried.
  static Promise<undefined> setDisplayLayout(
      sequence<DisplayLayout> layouts);

  // Enables/disables the unified desktop feature. If enabled while mirroring
  // is active, the desktop mode will not change until mirroring is turned
  // off. Otherwise, the desktop mode will switch to unified immediately.
  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  // |enabled|: True if unified desktop should be enabled.
  static undefined enableUnifiedDesktop(boolean enabled);

  // Starts overscan calibration for a display. This will show an overlay
  // on the screen indicating the current overscan insets. If overscan
  // calibration for display |id| is in progress this will reset calibration.
  // |id|: The display's unique identifier.
  static undefined overscanCalibrationStart(DOMString id);

  // Adjusts the current overscan insets for a display. Typically this should
  // either move the display along an axis (e.g. left+right have the same
  // value) or scale it along an axis (e.g. top+bottom have opposite values).
  // Each Adjust call is cumulative with previous calls since Start.
  // |id|: The display's unique identifier.
  // |delta|: The amount to change the overscan insets.
  static undefined overscanCalibrationAdjust(DOMString id, Insets delta);

  // Resets the overscan insets for a display to the last saved value (i.e
  // before Start was called).
  // |id|: The display's unique identifier.
  static undefined overscanCalibrationReset(DOMString id);

  // Complete overscan adjustments for a display  by saving the current values
  // and hiding the overlay.
  // |id|: The display's unique identifier.
  static undefined overscanCalibrationComplete(DOMString id);

  // Displays the native touch calibration UX for the display with |id| as
  // display id. This will show an overlay on the screen with required
  // instructions on how to proceed. The callback will be invoked in case of
  // successful calibration only. If the calibration fails, this will throw an
  // error.
  // |id|: The display's unique identifier.
  // |Returns|: Optional callback to inform the caller that the touch
  //      calibration has ended. The argument of the callback informs if the
  //      calibration was a success or not.
  // |PromiseValue|: success
  static Promise<boolean> showNativeTouchCalibration(DOMString id);

  // Starts custom touch calibration for a display. This should be called when
  // using a custom UX for collecting calibration data. If another touch
  // calibration is already in progress this will throw an error.
  // |id|: The display's unique identifier.
  static undefined startCustomTouchCalibration(DOMString id);

  // Sets the touch calibration pairs for a display. These |pairs| would be
  // used to calibrate the touch screen for display with |id| called in
  // startCustomTouchCalibration(). Always call |startCustomTouchCalibration|
  // before calling this method. If another touch calibration is already in
  // progress this will throw an error.
  // |pairs|: The pairs of point used to calibrate the display.
  // |bounds|: Bounds of the display when the touch calibration was performed.
  //     |bounds.left| and |bounds.top| values are ignored.
  static undefined completeCustomTouchCalibration(TouchCalibrationPairQuad pairs,
                                             Bounds bounds);

  // Resets the touch calibration for the display and brings it back to its
  // default state by clearing any touch calibration data associated with the
  // display.
  // |id|: The display's unique identifier.
  static undefined clearTouchCalibration(DOMString id);

  // Sets the display mode to the specified mirror mode. Each call resets the
  // state from previous calls. Calling setDisplayProperties() will fail for
  // the mirroring destination displays.
  // NOTE: This is only available to ChromeOS Kiosk apps and Web UI.
  // |info|: The information of the mirror mode that should be applied to the
  //     display mode.
  // |Returns|: Empty function called when the function finishes. To find out
  //     whether the function succeeded, $(ref:runtime.lastError) should be
  //     queried.
  static Promise<undefined> setMirrorMode(MirrorModeInfo info);

  // Fired when anything changes to the display configuration.
  static attribute OnDisplayChangedEvent onDisplayChanged;
};

partial interface System {
  static attribute Display display;
};

partial interface Browser {
  static attribute System system;
};

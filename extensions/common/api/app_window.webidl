// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

[instanceOf=Window]
typedef object DOMWindow;

[instanceOf=AppWindow]
typedef object AppWindowObject;

// Previously named Bounds.
dictionary ContentBounds {
  long left;
  long top;
  long width;
  long height;
};

dictionary BoundsSpecification {
  // The X coordinate of the content or window.
  long left;

  // The Y coordinate of the content or window.
  long top;

  // The width of the content or window.
  long width;

  // The height of the content or window.
  long height;

  // The minimum width of the content or window.
  long minWidth;

  // The minimum height of the content or window.
  long minHeight;

  // The maximum width of the content or window.
  long maxWidth;

  // The maximum height of the content or window.
  long maxHeight;
};

callback BoundsSetPositionCallback = undefined(long left, long top);
callback BoundsSetSizeCallback = undefined(long width, long height);
callback BoundsSetMinimumSizeCallback =
    undefined(long minWidth, long minHeight);
callback BoundsSetMaximumSizeCallback =
    undefined(long maxWidth, long maxHeight);

dictionary Bounds {
  // This property can be used to read or write the current X coordinate of
  // the content or window.
  required long left;

  // This property can be used to read or write the current Y coordinate of
  // the content or window.
  required long top;

  // This property can be used to read or write the current width of the
  // content or window.
  required long width;

  // This property can be used to read or write the current height of the
  // content or window.
  required long height;

  // This property can be used to read or write the current minimum width of
  // the content or window. A value of <code>null</code> indicates
  // 'unspecified'.
  long minWidth;

  // This property can be used to read or write the current minimum height of
  // the content or window. A value of <code>null</code> indicates
  // 'unspecified'.
  long minHeight;

  // This property can be used to read or write the current maximum width of
  // the content or window. A value of <code>null</code> indicates
  // 'unspecified'.
  long maxWidth;

  // This property can be used to read or write the current maximum height of
  // the content or window. A value of <code>null</code> indicates
  // 'unspecified'.
  long maxHeight;

  // Set the left and top position of the content or window.
  required BoundsSetPositionCallback setPosition;

  // Set the width and height of the content or window.
  required BoundsSetSizeCallback setSize;

  // Set the minimum size constraints of the content or window. The minimum
  // width or height can be set to <code>null</code> to remove the constraint.
  // A value of <code>undefined</code> will leave a constraint unchanged.
  required BoundsSetMinimumSizeCallback setMinimumSize;

  // Set the maximum size constraints of the content or window. The maximum
  // width or height can be set to <code>null</code> to remove the constraint.
  // A value of <code>undefined</code> will leave a constraint unchanged.
  required BoundsSetMaximumSizeCallback setMaximumSize;
};

dictionary FrameOptions {
  // Frame type: <code>none</code> or <code>chrome</code> (defaults to
  // <code>chrome</code>).
  //
  // For <code>none</code>, the <code>-webkit-app-region</code> CSS property
  // can be used to apply draggability to the app's window.
  //
  // <code>-webkit-app-region: drag</code> can be used to mark regions
  // draggable. <code>no-drag</code> can be used to disable this style on
  // nested elements.
  DOMString type;
  // Allows the frame color to be set. Frame coloring is only available if the
  // frame type is <code>chrome</code>.
  //
  // Frame coloring is new in Chrome 36.
  DOMString color;
  // Allows the frame color of the window when active to be set. Frame
  // coloring is only available if the frame type is <code>chrome</code>.
  //
  // Frame coloring is only available if the frame type is
  // <code>chrome</code>.
  //
  // Frame coloring is new in Chrome 36.
  DOMString activeColor;
  // Allows the frame color of the window when inactive to be set differently
  // to the active color. Frame
  // coloring is only available if the frame type is <code>chrome</code>.
  //
  // <code>inactiveColor</code> must be used in conjunction with <code>
  // color</code>.
  //
  // Frame coloring is new in Chrome 36.
  DOMString inactiveColor;
};

// State of a window: normal, fullscreen, maximized, minimized.
enum State {
  "normal",
  "fullscreen",
  "maximized",
  "minimized"
};

// Specifies the type of window to create.
enum WindowType {
  // Default window type.
  "shell",
  // OS managed window (Deprecated).
  "panel"
};

[noinline_doc] dictionary CreateWindowOptions {
  // Id to identify the window. This will be used to remember the size
  // and position of the window and restore that geometry when a window
  // with the same id is later opened.
  // If a window with a given id is created while another window with the same
  // id already exists, the currently opened window will be focused instead of
  // creating a new window.
  DOMString id;

  // Used to specify the initial position, initial size and constraints of the
  // window's content (excluding window decorations).
  // If an <code>id</code> is also specified and a window with a matching
  // <code>id</code> has been shown before, the remembered bounds will be used
  // instead.
  //
  // Note that the padding between the inner and outer bounds is determined by
  // the OS. Therefore setting the same bounds property for both the
  // <code>innerBounds</code> and <code>outerBounds</code> will result in an
  // error.
  //
  // This property is new in Chrome 36.
  BoundsSpecification innerBounds;

  // Used to specify the initial position, initial size and constraints of the
  // window (including window decorations such as the title bar and frame).
  // If an <code>id</code> is also specified and a window with a matching
  // <code>id</code> has been shown before, the remembered bounds will be used
  // instead.
  //
  // Note that the padding between the inner and outer bounds is determined by
  // the OS. Therefore setting the same bounds property for both the
  // <code>innerBounds</code> and <code>outerBounds</code> will result in an
  // error.
  //
  // This property is new in Chrome 36.
  BoundsSpecification outerBounds;

  // Default width of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long defaultWidth;

  // Default height of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long defaultHeight;

  // Default X coordinate of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long defaultLeft;

  // Default Y coordinate of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long defaultTop;

  // Width of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long width;

  // Height of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long height;

  // X coordinate of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long left;

  // Y coordinate of the window.
  [nodoc, deprecated="Use $(ref:BoundsSpecification)."] long top;

  // Minimum width of the window.
  [deprecated="Use innerBounds or outerBounds."] long minWidth;

  // Minimum height of the window.
  [deprecated="Use innerBounds or outerBounds."] long minHeight;

  // Maximum width of the window.
  [deprecated="Use innerBounds or outerBounds."] long maxWidth;

  // Maximum height of the window.
  [deprecated="Use innerBounds or outerBounds."] long maxHeight;

  // Type of window to create.
  [deprecated="All app windows use the 'shell' window type"] WindowType type;

  // Creates a special ime window. This window is not focusable and can be
  // stacked above virtual keyboard window. This is restriced to component ime
  // extensions.
  // Requires the <code>app.window.ime</code> API permission.
  [nodoc] boolean ime;

  // If true, the window will have its own shelf icon. Otherwise the window
  // will be grouped in the shelf with other windows that are associated with
  // the app. Defaults to false. If showInShelf is set to true you need to
  // specify an id for the window.
  boolean showInShelf;

  // URL of the window icon. A window can have its own icon when showInShelf
  // is set to true. The URL should be a global or an extension local URL.
  DOMString icon;

  // Frame type: <code>none</code> or <code>chrome</code> (defaults to
  // <code>chrome</code>). For <code>none</code>, the
  // <code>-webkit-app-region</code> CSS property can be used to apply
  // draggability to the app's window. <code>-webkit-app-region: drag</code>
  // can be used to mark regions draggable. <code>no-drag</code> can be used
  // to disable this style on nested elements.
  //
  // Use of <code>FrameOptions</code> is new in M36.
  (DOMString or FrameOptions) frame;

  // Size and position of the content in the window (excluding the titlebar).
  // If an id is also specified and a window with a matching id has been shown
  // before, the remembered bounds of the window will be used instead.
  [deprecated="Use innerBounds or outerBounds."] ContentBounds bounds;

  // Enable window background transparency.
  // Only supported in ash. Requires the <code>app.window.alpha</code> API
  // permission.
  [nodoc] boolean alphaEnabled;

  // The initial state of the window, allowing it to be created already
  // fullscreen, maximized, or minimized. Defaults to 'normal'.
  State state;

  // If true, the window will be created in a hidden state. Call show() on
  // the window to show it once it has been created. Defaults to false.
  boolean hidden;

  // If true, the window will be resizable by the user. Defaults to true.
  boolean resizable;

  // By default if you specify an id for the window, the window will only be
  // created if another window with the same id doesn't already exist. If a
  // window with the same id already exists that window is activated instead.
  // If you do want to create multiple windows with the same id, you can
  // set this property to false.
  [deprecated="Multiple windows with the same id is no longer supported."]
  boolean singleton;

  // If true, the window will stay above most other windows. If there are
  // multiple windows of this kind, the currently focused window will be in
  // the foreground. Requires the <code>alwaysOnTopWindows</code>
  // permission. Defaults to false.
  //
  // Call <code>setAlwaysOnTop()</code> on the window to change this property
  // after creation.
  boolean alwaysOnTop;

  // If true, the window will be focused when created. Defaults to true.
  boolean focused;

  // If true, and supported by the platform, the window will be visible on all
  // workspaces.
  boolean visibleOnAllWorkspaces;
};

callback AppWindowFocusCallback = undefined();
callback AppWindowFullscreenCallback = undefined();
callback AppWindowIsFullscreenCallback = boolean();
callback AppWindowMinimizeCallback = undefined();
callback AppWindowIsMinimizedCallback = boolean();
callback AppWindowMaximizeCallback = undefined();
callback AppWindowIsMaximizedCallback = boolean();
callback AppWindowRestoreCallback = undefined();
callback AppWindowMoveToCallback = undefined(long left, long top);
callback AppWindowResizeToCallback = undefined(long width, long height);
callback AppWindowDrawAttentionCallback = undefined();
callback AppWindowClearAttentionCallback = undefined();
callback AppWindowCloseCallback = undefined();
callback AppWindowShowCallback = undefined(optional boolean focused);
callback AppWindowHideCallback = undefined();
callback AppWindowGetBoundsCallback = ContentBounds?();
callback AppWindowSetBoundsCallback = undefined(ContentBounds bounds);
callback AppWindowSetIconCallback = undefined(DOMString iconUrl);
callback AppWindowIsAlwaysOnTopCallback = boolean();
callback AppWindowSetAlwaysOnTopCallback = undefined(boolean alwaysOnTop);
callback AppWindowAlphaEnabledCallback = boolean();
callback AppWindowSetVisibleOnAllWorkspacesCallback =
    undefined(boolean alwaysVisible);

[noinline_doc] dictionary AppWindow {
  // Focus the window.
  required AppWindowFocusCallback focus;

  // Fullscreens the window.
  //
  // The user will be able to restore the window by pressing ESC. An
  // application can prevent the fullscreen state to be left when ESC is
  // pressed by requesting the <code>app.window.fullscreen.overrideEsc</code>
  // permission and canceling the event by calling .preventDefault(), in the
  // keydown and keyup handlers, like this:
  //
  // <code>window.onkeydown = window.onkeyup = function(e) { if (e.keyCode ==
  // 27 /* ESC */) { e.preventDefault(); } };</code>
  //
  // Note <code>window.fullscreen()</code> will cause the entire window to
  // become fullscreen and does not require a user gesture. The HTML5
  // fullscreen API can also be used to enter fullscreen mode (see
  // <a href="http://developer.chrome.com/apps/api_other.html">Web APIs</a>
  // for more details).
  required AppWindowFullscreenCallback fullscreen;

  // Is the window fullscreen? This will be true if the window has been
  // created fullscreen or was made fullscreen via the
  // <code>AppWindow</code> or HTML5 fullscreen APIs.
  required AppWindowIsFullscreenCallback isFullscreen;

  // Minimize the window.
  required AppWindowMinimizeCallback minimize;

  // Is the window minimized?
  required AppWindowIsMinimizedCallback isMinimized;

  // Maximize the window.
  required AppWindowMaximizeCallback maximize;

  // Is the window maximized?
  required AppWindowIsMaximizedCallback isMaximized;

  // Restore the window, exiting a maximized, minimized, or fullscreen state.
  required AppWindowRestoreCallback restore;

  // Move the window to the position (|left|, |top|).
  [deprecated="Use outerBounds."] required AppWindowMoveToCallback moveTo;

  // Resize the window to |width|x|height| pixels in size.
  [deprecated="Use outerBounds."] required AppWindowResizeToCallback resizeTo;

  // Draw attention to the window.
  required AppWindowDrawAttentionCallback drawAttention;

  // Clear attention to the window.
  required AppWindowClearAttentionCallback clearAttention;

  // Close the window.
  required AppWindowCloseCallback close;

  // Show the window. Does nothing if the window is already visible.
  // Focus the window if |focused| is set to true or omitted.
  required AppWindowShowCallback show;

  // Hide the window. Does nothing if the window is already hidden.
  required AppWindowHideCallback hide;

  // Get the window's inner bounds as a $(ref:ContentBounds) object.
  [nocompile, deprecated="Use innerBounds or outerBounds."]
  required AppWindowGetBoundsCallback getBounds;

  // Set the window's inner bounds.
  [nocompile, deprecated="Use innerBounds or outerBounds."]
  required AppWindowSetBoundsCallback setBounds;

  // Set the app icon for the window (experimental).
  // Currently this is only being implemented on Ash.
  // TODO(stevenjb): Investigate implementing this on Windows and OSX.
  [nodoc] required AppWindowSetIconCallback setIcon;

  // Is the window always on top?
  required AppWindowIsAlwaysOnTopCallback isAlwaysOnTop;

  // Accessors for testing.
  [nodoc] required boolean hasFrameColor;
  [nodoc] required long activeFrameColor;
  [nodoc] required long inactiveFrameColor;

  // Set whether the window should stay above most other windows. Requires the
  // <code>alwaysOnTopWindows</code> permission.
  required AppWindowSetAlwaysOnTopCallback setAlwaysOnTop;

  // Can the window use alpha transparency?
  // TODO(jackhou): Document this properly before going to stable.
  [nodoc] required AppWindowAlphaEnabledCallback alphaEnabled;

  // Set whether the window is visible on all workspaces. (Only for platforms
  // that support this).
  required AppWindowSetVisibleOnAllWorkspacesCallback setVisibleOnAllWorkspaces;

  // The JavaScript 'window' object for the created child.
  required DOMWindow contentWindow;

  // The id the window was created with.
  required DOMString id;

  // The position, size and constraints of the window's content, which does
  // not include window decorations.
  // This property is new in Chrome 36.
  required Bounds innerBounds;

  // The position, size and constraints of the window, which includes window
  // decorations, such as the title bar and frame.
  // This property is new in Chrome 36.
  required Bounds outerBounds;
};

// Listener callback for the onBoundsChanged event.
callback OnBoundsChangedListener = undefined();

interface OnBoundsChangedEvent : ExtensionEvent {
  static undefined addListener(OnBoundsChangedListener listener);
  static undefined removeListener(OnBoundsChangedListener listener);
  static boolean hasListener(OnBoundsChangedListener listener);
};

// Listener callback for the onClosed event.
callback OnClosedListener = undefined();

interface OnClosedEvent : ExtensionEvent {
  static undefined addListener(OnClosedListener listener);
  static undefined removeListener(OnClosedListener listener);
  static boolean hasListener(OnClosedListener listener);
};

// Listener callback for the onFullscreened event.
callback OnFullscreenedListener = undefined();

interface OnFullscreenedEvent : ExtensionEvent {
  static undefined addListener(OnFullscreenedListener listener);
  static undefined removeListener(OnFullscreenedListener listener);
  static boolean hasListener(OnFullscreenedListener listener);
};

// Listener callback for the onMaximized event.
callback OnMaximizedListener = undefined();

interface OnMaximizedEvent : ExtensionEvent {
  static undefined addListener(OnMaximizedListener listener);
  static undefined removeListener(OnMaximizedListener listener);
  static boolean hasListener(OnMaximizedListener listener);
};

// Listener callback for the onMinimized event.
callback OnMinimizedListener = undefined();

interface OnMinimizedEvent : ExtensionEvent {
  static undefined addListener(OnMinimizedListener listener);
  static undefined removeListener(OnMinimizedListener listener);
  static boolean hasListener(OnMinimizedListener listener);
};

// Listener callback for the onRestored event.
callback OnRestoredListener = undefined();

interface OnRestoredEvent : ExtensionEvent {
  static undefined addListener(OnRestoredListener listener);
  static undefined removeListener(OnRestoredListener listener);
  static boolean hasListener(OnRestoredListener listener);
};

// Listener callback for the onAlphaEnabledChanged event.
callback OnAlphaEnabledChangedListener = undefined();

interface OnAlphaEnabledChangedEvent : ExtensionEvent {
  static undefined addListener(OnAlphaEnabledChangedListener listener);
  static undefined removeListener(OnAlphaEnabledChangedListener listener);
  static boolean hasListener(OnAlphaEnabledChangedListener listener);
};

// Use the <code>chrome.app.window</code> API to create windows. Windows
// have an optional frame with title bar and size controls. They are not
// associated with any Chrome browser windows. See the <a
// href="https://github.com/GoogleChrome/chrome-app-samples/tree/master/samples/window-state">
// Window State Sample</a> for a demonstration of these options.
interface Window {
  // The size and position of a window can be specified in a number of
  // different ways. The most simple option is not specifying anything at
  // all, in which case a default size and platform dependent position will
  // be used.
  //
  // To set the position, size and constraints of the window, use the
  // <code>innerBounds</code> or <code>outerBounds</code> properties. Inner
  // bounds do not include window decorations. Outer bounds include the
  // window's title bar and frame. Note that the padding between the inner and
  // outer bounds is determined by the OS. Therefore setting the same property
  // for both inner and outer bounds is considered an error (for example,
  // setting both <code>innerBounds.left</code> and
  // <code>outerBounds.left</code>).
  //
  // To automatically remember the positions of windows you can give them ids.
  // If a window has an id, This id is used to remember the size and position
  // of the window whenever it is moved or resized. This size and position is
  // then used instead of the specified bounds on subsequent opening of a
  // window with the same id. If you need to open a window with an id at a
  // location other than the remembered default, you can create it hidden,
  // move it to the desired location, then show it.
  //
  // |Returns|: Called in the creating window (parent) before the load event is
  // called in the created window (child). The parent can set fields or
  // functions on the child usable from onload. E.g. background.js:
  //
  // <code>function(createdWindow) { createdWindow.contentWindow.foo =
  // function () { }; };</code>
  //
  // window.js:
  //
  // <code>window.onload = function () { foo(); }</code>
  // |PromiseValue|: createdWindow
  static Promise<AppWindowObject> create(
      DOMString url,
      optional CreateWindowOptions options);

  // Returns an $(ref:AppWindow) object for the
  // current script context (ie JavaScript 'window' object). This can also be
  // called on a handle to a script context for another page, for example:
  // otherWindow.chrome.app.window.current().
  [nocompile] static AppWindow? current();
  [nocompile, nodoc] static undefined initializeAppWindow(object state);

  // Gets an array of all currently created app windows. This method is new in
  // Chrome 33.
  [nocompile] static sequence<AppWindow> getAll();

  // Gets an $(ref:AppWindow) with the given id. If no window with the given id
  // exists null is returned. This method is new in Chrome 33.
  [nocompile] static AppWindow? get(DOMString id);

  // Whether the current platform supports windows being visible on all
  // workspaces.
  [nocompile] static boolean canSetVisibleOnAllWorkspaces();

  // Fired when the window is resized.
  [nocompile] static attribute OnBoundsChangedEvent onBoundsChanged;

  // Fired when the window is closed. Note, this should be listened to from
  // a window other than the window being closed, for example from the
  // background page. This is because the window being closed will be in the
  // process of being torn down when the event is fired, which means not all
  // APIs in the window's script context will be functional.
  [nocompile] static attribute OnClosedEvent onClosed;

  // Fired when the window is fullscreened (either via the
  // <code>AppWindow</code> or HTML5 APIs).
  [nocompile] static attribute OnFullscreenedEvent onFullscreened;

  // Fired when the window is maximized.
  [nocompile] static attribute OnMaximizedEvent onMaximized;

  // Fired when the window is minimized.
  [nocompile] static attribute OnMinimizedEvent onMinimized;

  // Fired when the window is restored from being minimized or maximized.
  [nocompile] static attribute OnRestoredEvent onRestored;

  // Fired when the window's ability to use alpha transparency changes.
  [nocompile, nodoc]
  static attribute OnAlphaEnabledChangedEvent onAlphaEnabledChanged;
};

partial interface App {
  static attribute Window window;
};

partial interface Browser {
  static attribute App app;
};

#  Windows 10 Virtual Desktop support

Windows 10 introduced Virtual Desktop support. Virtual Desktops are similar to
Chrome OS and Mac workspaces. A virtual desktop is a collection of windows.
Every window belongs to a virtual desktop.  When a virtual desktop is selected
to be active, the windows associated with that virtual desktop are displayed on
the screen. When a virtual desktop is hidden, all of its windows are also
hidden. This enables the user to create multiple working environments and to
switch between them. An app (e.g., Chromium) can have windows open on
different virtual desktops, and thus may need to be Virtual Desktop-aware.

The user-facing Chromium support for virtual desktops consists of two things:

  * When launching the browser with session restore, browser windows are moved
  to the virtual desktop they were on when the browser shutdown.
  * When opening a URL with the browser, either open it in a window on the
  current virtual desktop, or open a new window on the current virtual desktop.
  Don't open it in a tab in a window on another virtual desktop.

The core UI principles are that windows should be restored to the desktop they
were shut down on, and opening an app window shouldn't change the current
virtual desktop. Only the user should be able to change virtual desktops, or
move windows between virtual desktops.

Windows 10 exposes the COM interface
[IVirtualDesktopManager](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ivirtualdesktopmanager)
to access the Virtual Desktop functionality. To make sure that opening a URL
stays on the current virtual desktop,
[BrowserView::IsOnCurrentWorkspace](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/frame/browser_view.cc?q=%20BrowserView::IsOnCurrentWorkspace)
uses the IVirtualDesktopManager method
[IsWindowOnCurrentVirtualDesktop](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-iswindowoncurrentvirtualdesktop).
[BrowserMatches](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/browser_finder.cc?q=BrowserMatches)
in browser_finder.cc only returns a browser window on the current desktop.

To restore browser windows to the desktop they were last open on,
[BrowserDesktopWindowTreeHostWin](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.cc)
implements GetWorkspace by using the
[GetWindowDesktopId](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-getwindowdesktopid) method on
IVirtualDesktopManager, and restores the workspace using
[MoveWindowToDesktop](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ivirtualdesktopmanager-movewindowtodesktop),
in its ::Init method.

The actual implementation is a bit more complicated in order to avoid
calling COM methods on the UI thread, or destroying COM objects on the UI
thread, since doing so can cause nested message loops and re-entrant calls,
leading to blocked UI threads and crashes. The
[VirtualDesktopHelper](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/views/frame/browser_desktop_window_tree_host_win.cc?q=VirtualDesktopHelper&sq=&ss=chromium%2Fchromium%2Fsrc)
class does the workspace handling for BrowserDesktopWindowTreeHostWin, including
doing all the COM operations on a separate COM task runner. The GetWorkspace
method is synchronous so VirtualDesktopHelper has to remember and return the
most recent virtual desktop. Windows has no notification of a window changing
virtual desktops, so BrowserDesktopWindowTreeHostWin updates the virtual desktop
of a window whenever it gets focus, and if it has changed, calls
WindowTreeHost::OnHostWorkspaceChanged. This means that if a window is moved
to a different virtual desktop, but doesn't get focus before the browser is shut
down, the browser window will be restored to the previous virtual desktop.

Windows on different virtual desktops share the same coordinate system, so code
that iterates over windows generally needs to be Virtual Desktop-aware.
For example, the following places in code ignore windows not on the current
virtual desktop:

 * [LocalProcessWindowFinder::GetProcessWindowAtPoint](https://source.chromium.org/chromium/chromium/src/+/main:ui/display/win/local_process_window_finder_win.cc?q=LocalProcessWindowFinder::ShouldStopIterating&ss=chromium%2Fchromium%2Fsrc)
 * third_party/webrtc/modules/desktop_capture/win/window_capture_utils.cc
 * [Native Window occlusion tracker](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/native_window_occlusion_tracker_win.cc?q=WindowCanOccludeOtherWindowsOnCurrentVirtualDesktop&ss=chromium%2Fchromium%2Fsrc),
 when determining if a Chromium window is occluded/covered by other windows.
 Windows not on the current virtual desktop are considered occluded.



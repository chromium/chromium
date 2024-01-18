# Bitmap Pipeline

This pages details how bitmaps are moved from the renderer to the screen.

The renderer can request two different operations from the browser:
* PaintRect: a bitmap to be painted at a given location on the screen
* Scroll: a horizontal or vertical scroll of the screen, and a bitmap to painted

Across all three platforms, shared memory is used to transport the bitmap from
the renderer to the browser. On Windows, a shared section is used. On Linux,
it's SysV shared memory and on the Mac we use POSIX shared memory.

Windows and Linux create shared memory in the renderer process. On Mac, since
the renderer is sandboxed, it cannot create shared memory segments and uses a
synchronous IPC to the browser to create them (ViewHostMsg\_AllocTransportDIB).
These shared memory segments are called TranportDIBs (device independent
bitmaps) in the code.

Transport DIBs are allocated on demand by the render\_process and cached
therein, in a two entry cache. The IPC messages to the browser contain a
TransportDIB::Id which names a transport DIB. In the case of Mac, since the
browser created them in the first place, it keeps a map of all allocated
transport DIBs in the RenderProcessHost. The ids on the wire are then the inode
numbers of the shared memory segments.

On Windows, the Id is the HANDLE value from the renderer process. On Linux the
id is the SysV key. Thus, on both Windows and Linux, the id is sufficient to map
the transport DIB, while on Mac is not. This is why, on Mac, the browser
keeps handles to all the possible transport DIBs.

Each RenderProcessHost keeps a small cache of recently used transport DIBs. This
means that, when many paint operations are performed in succession, the same
shared memory should be reused (as long as it's large enough). Also, this shared
memory should remain mapped in both the renderer and browser process, reduci ng
the amount of VM churn.

The transport DIB caches in both the renderer and browser are flushed after some
period of inactivity, currently five seconds.

### Backing stores

Backing stores are browser side copies of the current RenderView bitmap. The
renderer sends paints to the browser to update small portions of the backing
store but, for performance reasons, when we want to repaint the whole thing
(i.e. because we switched tabs) we don't want to go to the renderer to redraw it
all.

On Windows and Mac, the backing store is kept in heap memory in the browser. On
Windows, we use one advantage which is that we can use Win32 calls to scroll
both the window and the backing store. This is faster than scrolling ourselves
and redrawing everything to the window.

On Mac, the backing store is a Skia bitmap and we do the scrolling ourselves.

On Linux, the backing store is kept on the X server. It's a large X pixmap and
we handle exposes by directing the X server to copy from this pixmap. This means
that we can repaint the window without sending any bitmaps to the X server. It
also means that we can perform optimised scrolling by directing the X server to
scroll the window and pixmap for us.

Having backing stores on the X server is a major win in the case of remote X.

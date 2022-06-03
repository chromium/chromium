# Linux Sandbox IPC

The Sandbox IPC system is separate from the 'main' IPC system. The sandbox IPC
is a lower level system which deals with cases where we need to route requests
from the bottom of the call stack up into the browser.

The motivating example used to be Skia, which uses fontconfig to load
fonts. Howvever, the OOP IPC for FontConfig was moved to using Font Service and
the `components/services/font/public/cpp/font_loader.h` interface.

These days, only the out-of-process localtime implementation as well as
an OOP call for making a shared memory segment are using the Sandbox IPC
file-descriptor based system. See `sandbox/linux/services/libc_interceptor.cc`.

Thus we define a small IPC system which doesn't depend on anything but `base`
and which can make synchronous requests to the browser process.

The [zygote](zygote.md) starts with a `UNIX DGRAM` socket installed in a
well known file descriptor slot (currently 4). Requests can be written to this
socket which are then processed on a special "sandbox IPC" process. Requests
have a magic `int` at the beginning giving the type of the request.

All renderers share the same socket, so replies are delivered via a reply
channel which is passed as part of the request. So the flow looks like:

1.  The renderer creates a `UNIX DGRAM` socketpair.
1.  The renderer writes a request to file descriptor 4 with an `SCM_RIGHTS`
    control message containing one end of the fresh socket pair.
1.  The renderer blocks reading from the other end of the fresh socketpair.
1.  A special "sandbox IPC" process receives the request, processes it and
    writes the reply to the end of the socketpair contained in the request.
1.  The renderer wakes up and continues.

The browser side of the processing occurs in
`chrome/browser/renderer_host/render_sandbox_host_linux.cc`. The renderer ends
could occur anywhere, but the browser side has to know about all the possible
requests so that should be a good starting point.

Here is a (possibly incomplete) list of endpoints in the renderer:

### localtime

`content/browser/sandbox_ipc_linux.h` defines HandleLocalTime which is
implemented in `sandbox/linux/services/libc_interceptor.cc`.

### Creating a shared memory segment

`content/browser/sandbox_ipc_linux.h` defines HandleMakeSharedMemorySegment
which is implemented in `content/browser/sandbox_ipc_linux.cc`.

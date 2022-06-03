# Debugging GPU related code

Chromium's GPU system is multi-process, which can make debugging it rather
difficult. See [GPU Command Buffer] for some of the nitty gitty. These are just
a few notes to help with debugging.

[TOC]

<!-- TODO(kainino): update link if the page moves -->
[GPU Command Buffer]: https://sites.google.com/a/chromium.org/dev/developers/design-documents/gpu-command-buffer

## Renderer Process Code

### `--enable-gpu-client-logging`

If you are trying to track down a bug in a GPU client process (compositing,
WebGL, Skia/Ganesh, Aura), then in a debug build you can use the
`--enable-gpu-client-logging` flag, which will show every GL call sent to the
GPU service process. (From the point of view of a GPU client, it's calling
OpenGL ES functions - but the real driver calls are made in the GPU process.)

You can also use this flag in a release build by specifying the GN argument:

```
enable_gpu_client_logging=true
```

It's typically necessary to specify the `--enable-logging=stderr` flag as well:

```
--enable-gpu-client-logging --enable-logging=stderr
```

The output looks like this:

```
[4782:4782:1219/141706:INFO:gles2_implementation.cc(1026)] [.WebGLRenderingContext] glUseProgram(3)
[4782:4782:1219/141706:INFO:gles2_implementation_impl_autogen.h(401)] [.WebGLRenderingContext] glGenBuffers(1, 0x7fffc9e1269c)
[4782:4782:1219/141706:INFO:gles2_implementation_impl_autogen.h(416)]   0: 1
[4782:4782:1219/141706:INFO:gles2_implementation_impl_autogen.h(23)] [.WebGLRenderingContext] glBindBuffer(GL_ARRAY_BUFFER, 1)
[4782:4782:1219/141706:INFO:gles2_implementation.cc(1313)] [.WebGLRenderingContext] glBufferData(GL_ARRAY_BUFFER, 36, 0x7fd268580120, GL_STATIC_DRAW)
[4782:4782:1219/141706:INFO:gles2_implementation.cc(2480)] [.WebGLRenderingContext] glEnableVertexAttribArray(0)
[4782:4782:1219/141706:INFO:gles2_implementation.cc(1140)] [.WebGLRenderingContext] glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0)
[4782:4782:1219/141706:INFO:gles2_implementation_impl_autogen.h(135)] [.WebGLRenderingContext] glClear(16640)
[4782:4782:1219/141706:INFO:gles2_implementation.cc(2490)] [.WebGLRenderingContext] glDrawArrays(GL_TRIANGLES, 0, 3)
```

### Checking about:gpu

The GPU process logs many errors and warnings. You can see these by navigating
to `about:gpu`. Logs appear at the bottom of the page. You can also see them
on standard output if Chromium is run from the command line on Linux/Mac.
On Windows, you need debugging tools (like VS, WinDbg, etc.) to connect to the
debug output stream.

**Note:** If `about:gpu` is telling you that your GPU is disabled and
hardware acceleration is unavailable, it might be a problem with your GPU being
unsupported. To override this and turn on hardware acceleration anyway, you can
use the `--ignore-gpu-blocklist` command line option when starting Chromium.

### Breaking on GL Error

In <code>[gles2_implementation.h]</code>, there is some code like this:

```cpp
// Set to 1 to have the client fail when a GL error is generated.
// This helps find bugs in the renderer since the debugger stops on the error.
#if DCHECK_IS_ON()
#if 0
#define GL_CLIENT_FAIL_GL_ERRORS
#endif
#endif
```

Change that `#if 0` to `#if 1`, build a debug build, then run in a debugger.
The debugger will break when any renderer code sees a GL error, and you should
be able to examine the call stack to find the issue.

[gles2_implementation.h]: https://chromium.googlesource.com/chromium/src/+/main/gpu/command_buffer/client/gles2_implementation.h

### Labeling your calls

The output of all of the errors, warnings and debug logs are prefixed. You can
set this prefix by calling `glPushGroupMarkerEXT`, `glPopGroupMarkerEXT` and
`glInsertEventMarkerEXT`. `glPushGroupMarkerEXT` appends a string to the end of
the current log prefix (think namespace in C++). `glPopGroupmarkerEXT` pops off
the last string appended. `glInsertEventMarkerEXT` sets a suffix for the
current string. Example:

```cpp
glPushGroupMarkerEXT(0, "Foo");        // -> log prefix = "Foo"
glInsertEventMarkerEXT(0, "This");     // -> log prefix = "Foo.This"
glInsertEventMarkerEXT(0, "That");     // -> log prefix = "Foo.That"
glPushGroupMarkerEXT(0, "Bar");        // -> log prefix = "Foo.Bar"
glInsertEventMarkerEXT(0, "Orange");   // -> log prefix = "Foo.Bar.Orange"
glInsertEventMarkerEXT(0, "Banana");   // -> log prefix = "Foo.Bar.Banana"
glPopGroupMarkerEXT();                 // -> log prefix = "Foo.That"
```

### Making a reduced test case.

You can often make a simple OpenGL-ES-2.0-only C++ reduced test case that is
relatively quick to compile and test, by adding tests to the `gl_tests` target.
Those tests exist in `src/gpu/command_buffer/tests` and are made part of the
build in `src/gpu/BUILD.gn`. Build with `ninja -C out/Debug gl_tests`. All the
same command line options listed on this page will work with the `gl_tests`,
plus `--gtest_filter=NameOfTest` to run a specific test. Note the `gl_tests`
are not multi-process, so they probably won't help with race conditions, but
they do go through most of the same code and are much easier to debug.

### Debugging the renderer process

Given that Chrome starts many renderer processes I find it's easier if I either
have a remote webpage I can access or I make one locally and then use a local
server to serve it like `python -m SimpleHTTPServer`. Then

On Linux this works for me:

*   `out/Debug/chromium --no-sandbox --renderer-cmd-prefix="xterm -e gdb
    --args" http://localhost:8000/page-to-repro.html`

On OSX this works for me:

*   `out/Debug/Chromium.app/Contents/MacOSX/Chromium --no-sandbox
    --renderer-cmd-prefix="xterm -e gdb --args"
    http://localhost:8000/page-to-repro.html`

On Windows I use `--renderer-startup-dialog` and then connect to the listed process.

Note 1: On Linux and OSX I use `cgdb` instead of `gdb`.

Note 2: GDB can take minutes to index symbol. To save time, you can precache
that computation by running `build/gdb-add-index out/Debug/chrome`.

## GPU Process Code

### `--enable-gpu-service-logging`

In a debug build or a release build with dcheck_always_on=true in GN argument,
this will print all actual calls into the GL driver.

To use it in Release builds without dcheck_always_on = true, specify GN argument
enable_gpu_service_logging=true.

For non-rooted devices running production builds, we can not set the command
line flags. Use about://flags 'Enable gpu service logging' instead.

```
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kEnableVertexAttribArray
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(905)] glEnableVertexAttribArray(0)
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kVertexAttribPointer
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(1573)] glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0)
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kClear
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(746)] glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(840)] glDepthMask(GL_TRUE)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(900)] glEnable(GL_DEPTH_TEST)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(1371)] glStencilMaskSeparate(GL_FRONT, 4294967295)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(1371)] glStencilMaskSeparate(GL_BACK, 4294967295)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(860)] glDisable(GL_STENCIL_TEST)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(860)] glDisable(GL_CULL_FACE)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(860)] glDisable(GL_SCISSOR_TEST)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(900)] glEnable(GL_BLEND)
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(721)] glClear(16640)
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kDrawArrays
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(870)] glDrawArrays(GL_TRIANGLES, 0, 3)
```

Note that GL calls into the driver are not currently prefixed (todo?). But, you
can tell from the commands logged which command, from which context caused the
following GL calls to be made.

Also note that client resource IDs are virtual IDs, so calls into the real GL
driver will not match (though some commands print the mapping). Examples:

```
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kBindTexture
[5497:5497:1219/142413:INFO:gles2_cmd_decoder.cc(837)] [.WebGLRenderingContext] glBindTexture: client_id = 2, service_id = 10
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(662)] glBindTexture(GL_TEXTURE_2D, 10)
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [0052064A367F0000]cmd: kBindBuffer
[5497:5497:1219/142413:INFO:gles2_cmd_decoder.cc(837)] [0052064A367F0000] glBindBuffer: client_id = 2, service_id = 6
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(637)] glBindBuffer(GL_ARRAY_BUFFER, 6)
[5497:5497:1219/142413:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kBindFramebuffer
[5497:5497:1219/142413:INFO:gles2_cmd_decoder.cc(837)] [.WebGLRenderingContext] glBindFramebuffer: client_id = 1, service_id = 3
[5497:5497:1219/142413:INFO:gl_bindings_autogen_gl.cc(652)] glBindFramebufferEXT(GL_FRAMEBUFFER, 3)
```

etc... so that you can see renderer process code would be using the client IDs
where as the gpu process is using the service IDs. This is useful for matching
up calls if you're dumping both client and service GL logs.

### `--enable-gpu-debugging`

In any build, this will call glGetError after each command

### `--enable-gpu-command-logging`

This will print the name of each GPU command before it is executed.

```
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kBindBuffer
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kBufferData
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: SetToken
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kEnableVertexAttribArray
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kVertexAttribPointer
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kClear
[5234:5234:1219/052139:ERROR:gles2_cmd_decoder.cc(3301)] [.WebGLRenderingContext]cmd: kDrawArrays
```

### Debugging in the GPU Process

Given the multi-processness of chromium it can be hard to debug both sides.
Turning on all the logging and having a small test case is useful. One minor
suggestion, if you have some idea where the bug is happening a call to some
obscure gl function like `glHint()` can give you a place to catch a command
being processed in the GPU process (put a break point on
`gpu::gles2::GLES2DecoderImpl::HandleHint`. Once in you can follow the commands
after that. All of them go through `gpu::gles2::GLES2DecoderImpl::DoCommand`.

To actually debug the GPU process:

On Linux this works for me:

*   `out/Debug/chromium --no-sandbox --gpu-launcher="xterm -e gdb --args"
    http://localhost:8000/page-to-repro.html`

On OSX this works for me:

*   `out/Debug/Chromium.app/Contents/MacOSX/Chromium --no-sandbox
    --gpu-launcher="xterm -e gdb --args"
    http://localhost:8000/page-to-repro.html`

On Windows I use `--gpu-startup-dialog` and then connect to the listed process.

### `GPU PARSE ERROR`

If you see this message in `about:gpu` or your console and you didn't cause it
directly (by calling `glLoseContextCHROMIUM`) and it's something other than 5
that means there's likely a bug. Please file an issue at <http://crbug.com/new>.

## Tracing OpenGL calls

Passing the command line flag `--enable-gpu-service-tracing` causes
the GPU process to emit one trace event per OpenGL API call. (See
"Debugging Performance", below.) This is useful when trying to
understand where the expensive operations are in a given set of work
sent from a renderer process to the GPU process, and processed
underneath `CommandBufferService::PutChanged`.

## Debugging Performance

If you have something to add here please add it. Most perf debugging is done
using `about:tracing` (see [Trace Event Profiling] for details). Otherwise,
be aware that, since the system is multi-process, calling:

```
start = GetTime()
DoSomething()
glFinish()
end = GetTime
printf("elapsedTime = %f\n", end - start);
```

**will not** give you meaningful results.

[Trace Event Profiling]: https://sites.google.com/a/chromium.org/dev/developers/how-tos/trace-event-profiling-tool

.. _devguide-coding-3D-graphics:

.. include:: /migration/deprecation.inc

###########
3D Graphics
###########

Native Client applications use the `OpenGL ES 2.0
<http://en.wikipedia.org/wiki/OpenGL_ES>`_ API for 3D rendering. This document
describes how to call the OpenGL ES 2.0 interface in a Native Client module and
how to build an efficient rendering loop. It also explains how to validate GPU
drivers and test for specific GPU capabilities, and provides tips to help ensure
your rendering code runs efficiently.

.. Note::
  :class: note

  **Note**: 3D drawing and OpenGL are complex topics. This document deals only
  with issues directly related to programming in the Native Client
  environment. To learn more about OpenGL ES 2.0 itself, see the `OpenGL ES 2.0
  Programming Guide <http://opengles-book.com/>`_.

Validating the client graphics platform
=======================================

Native Client is a software technology that lets you code an application once
and run it on multiple platforms without worrying about the implementation
details on every possible target platform. It's difficult to provide the same
support at the hardware level. Graphics hardware comes from many different
manufacturers and is controlled by drivers of varying quality. A particular GPU
driver may not support every OpenGL ES 2.0 feature, and some drivers are known
to have vulnerabilities that can be exploited.

Even if the GPU driver is safe to use, your program should perform a validation
check before you launch your application to ensure that the driver supports all
the features you need.

Vetting the driver in JavaScript
--------------------------------

At startup, the application should perform a few additional tests that can be
implemented in JavaScript on its hosting web page. The script that performs
these tests should be included before the module's ``embed`` tag, and ideally
the ``embed`` tag should appear on the hosting page only if these tests succeed.

The first thing to check is whether you can create a graphics context. If you
can, use the context to confirm the existence of any required OpenGL ES 2.0
extensions.  You may want to refer to the `extension registry
<http://www.khronos.org/registry/webgl/extensions/>`_ and include `vendor
prefixes <https://developer.mozilla.org/en-US/docs/WebGL/Using_Extensions>`_
when checking for extensions.

Vetting the driver in Native Client
-----------------------------------

Create a context
^^^^^^^^^^^^^^^^

Once you've passed the JavaScript validation tests, it's safe to add a Native
Client embed tag to the hosting web page and load the module. As part of the
module initialization code, you must create a graphics context for the app by
either creating a C++ ``Graphics3D`` object or calling ``PPB_Graphics3D`` API
function ``Create``. Don't assume this will always succeed; you still might have
problems creating the context. If you are in development mode and can't create
the context, try creating a simpler version to see if you're asking for an
unsupported feature or exceeding a driver resource limit. Your production code
should always check that the context was created and fail gracefully if that's
not the case.

Check for extensions and capabilities
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Not every GPU supports every extension or has the same amount of texture units,
vertex attributes, etc. On startup, call ``glGetString(GL_EXTENSIONS)`` and
check for the extensions and the features you need. For example:

* If you are using non power-of-2 texture with mipmaps, make sure
  ``GL_OES_texture_npot`` exists.

* If you are using floating point textures, make sure ``GL_OES_texture_float``
  exists.

* If you are using DXT1, DXT3, or DXT5 textures, make sure the corresponding
  extensions ``GL_ANGLE_texture_compression_dxt1``,
  ``GL_ANGLE_texture_compression_dxt3``, and
  ``GL_ANGLE_texture_compression_dxt5`` exist.

* If you are using the functions ``glDrawArraysInstancedANGLE``,
  ``glDrawElementsInstancedANGLE``, ``glVertexAttribDivisorANGLE``, or the PPAPI
  interface ``PPB_OpenGLES2InstancedArrays``, make sure the corresponding
  extension ``GL_ANGLE_instanced_arrays`` exists.

* If you are using the function ``glRenderbufferStorageMultisampleEXT``, or the
  PPAPI interface ``PPB_OpenGLES2FramebufferMultisample``, make sure the
  corresponding extension ``GL_CHROMIUM_framebuffer_multisample`` exists.

* If you are using the functions ``glGenQueriesEXT``, ``glDeleteQueriesEXT``,
  ``glIsQueryEXT``, ``glBeginQueryEXT``, ``glEndQueryEXT``, ``glGetQueryivEXT``,
  ``glGetQueryObjectuivEXT``, or the PPAPI interface ``PPB_OpenGLES2Query``,
  make sure the corresponding extension ``GL_EXT_occlusion_query_boolean``
  exists.

* If you are using the functions ``glMapBufferSubDataCHROMIUM``,
  ``glUnmapBufferSubDataCHROMIUM``, ``glMapTexSubImage2DCHROMIUM``,
  ``glUnmapTexSubImage2DCHROMIUM``, or the PPAPI interface
  ``PPB_OpenGLES2ChromiumMapSub``, make sure the corresponding extension
  ``GL_CHROMIUM_map_sub`` exists.

Check for system capabilites with ``glGetIntegerv`` and adjust shader programs
as well as texture and vertex data accordingly:

* If you are using textures in vertex shaders, make sure
  ``glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, ...)`` and
  ``glGetIntegerv(GL_MAX_TEXTURE_SIZE, ...)`` return values greater than 0.

* If you are using more than 8 textures in a single shader, make sure
  ``glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, ...)`` returns a value greater
  than or equal to the number of simultaneous textures you need.

Vetting the driver in the Chrome Web Store
------------------------------------------

If you choose to place your application in the `Chrome Web Store </webstore>`_,
its Web Store `manifest file </extensions/manifest>`_ can include the ``webgl``
feature in the requirements parameter. It looks like this:

.. naclcode::

  "requirements": {
    "3D": {
      "features": ["webgl"]
    }
  }

While WebGL is technically a JavaScript API, specifying the ``webgl`` feature
also works for OpenGL ES 2.0 because both interfaces use the same driver.

This manifest item is not required, but if you include it, the Chrome Web Store
will prevent a user from installing the application if the browser is running on
a machine that does not support OpenGL ES 2.0 or that is using a known
blacklisted GPU driver that could invite an attack.

If the Web Store determines that the user's driver is deficient, the app won't
appear on the store's tile display. However, it will appear in store search
results or if the user links to it directly, in which case the user could still
download it. But the manifest requirements will be checked when the user reaches
the install page, and if there is a problem, the browser will display the
message "This application is not supported on this computer. Installation has
been disabled."

The manifest-based check applies only to downloads directly from the Chrome Web
Store. It is not performed when an application is loaded via `inline
installation </webstore/inline_installation>`_.

What to do when there are problems
----------------------------------

Using the vetting procedure described above, you should be able to detect the
most common problems before your application runs. If there are problems, your
code should describe the issue as clearly as possible. That's easy if there is a
missing feature. Failure to create a graphics context is tougher to diagnose. At
the very least, you can suggest that the user try to update the driver.  You
might want to linke to the Chrome page that describes `how to do updates
<https://support.google.com/chrome/answer/1202946>`_.

If a user can't update the driver, or their problem persists, be sure to gather
information about their graphics environment. Ask for the contents of the Chrome
``about:gpu`` page.

Document unreliable drivers
---------------------------

It can be helpful to include information about known dubious drivers in your
user documentation. This might help identify if a rogue driver is the cause of a
problem. There are many sources of GPU driver blacklists. Two such lists can be
found at the `Chromium project
<http://src.chromium.org/viewvc/chrome/trunk/deps/gpu/software_rendering_list/software_rendering_list.json>`_
and `Khronos <http://www.khronos.org/webgl/wiki/BlacklistsAndWhitelists>`_. You
can use these lists to include information in your documentation that warns
users about dangerous drivers.

Test your defenses
------------------

You can test your driver validation code by running Chrome with the following
flags (all at once) and watching how your application responds:

* ``--disable-webgl``
* ``--disable-pepper-3d``
* ``--disable_multisampling``
* ``--disable-accelerated-compositing``
* ``--disable-accelerated-2d-canvas``

Calling OpenGL ES 2.0 commands
==============================

There are three ways to write OpenGL ES 2.0 calls in Native Client.

Use "pure" OpenGL ES 2.0 function calls
---------------------------------------

You can make OpenGL ES 2.0 calls through a Pepper extension library.  The SDK
example ``examples/api/graphics_3d`` works this way.  In the file
``graphics_3d.cc``, the key initialization steps are as follows:

* Add these includes at the top of the file:

  .. naclcode::

    #include <GLES2/gl2.h>
    #include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

* Define the function ``InitGL``. The exact specification of ``attrib_list``
  will be application specific.

  .. naclcode::

    bool InitGL(int32_t new_width, int32_t new_height) {
      if (!glInitializePPAPI(pp::Module::Get()->get_browser_interface())) {
        fprintf(stderr, "Unable to initialize GL PPAPI!\n");
        return false;
      }

      const int32_t attrib_list[] = {
        PP_GRAPHICS3DATTRIB_ALPHA_SIZE, 8,
        PP_GRAPHICS3DATTRIB_DEPTH_SIZE, 24,
        PP_GRAPHICS3DATTRIB_WIDTH, new_width,
        PP_GRAPHICS3DATTRIB_HEIGHT, new_height,
        PP_GRAPHICS3DATTRIB_NONE
      };

      context_ = pp::Graphics3D(this, attrib_list);
      if (!BindGraphics(context_)) {
        fprintf(stderr, "Unable to bind 3d context!\n");
        context_ = pp::Graphics3D();
        glSetCurrentContextPPAPI(0);
        return false;
      }

      glSetCurrentContextPPAPI(context_.pp_resource());
      return true;
    }

* Include logic in ``Instance::DidChangeView`` to call ``InitGL`` whenever
  necessary: upon application launch (when the graphics context is NULL) and
  whenever the module's View changes size.

Use Regal
---------

If you are porting an OpenGL ES 2.0 application, or are comfortable writing in
OpenGL ES 2.0, you should stick with the Pepper APIs or pure OpenGL ES 2.0 calls
described above. If you are porting an application that uses features not in
OpenGL ES 2.0, consider using Regal. Regal is an open source library that
supports many versions of OpenGL. Regal recently added support for Native
Client. Regal forwards most OpenGL calls directly to the underlying graphics
library, but it can also emulate other calls that are not included (when
hardware support exists). See `libregal
<http://www.altdevblogaday.com/2012/09/04/bringing-regal-opengl-to-native-client/>`_
for more info.

Use the Pepper API
------------------

Your code can call the Pepper PPB_OpenGLES2 API directly, as with any Pepper
interface. When you write in this way, each invocation of an OpenGL ES 2.0
function must begin with a reference to the Pepper interface, and the first
argument is the graphics context. To invoke the function ``glCompileShader``,
your code might look like:

.. naclcode::

  ppb_g3d_interface->CompileShader(graphicsContext, shader);

This approach specifically targets the Pepper APIs. Each call corresponds to a
OpenGL ES 2.0 function, but the syntax is unique to Native Client, so the source
file is not portable.

Implementing a rendering loop
=============================

Graphics applications require a continuous frame render-and-redraw cycle that
runs at a high frequency. To achieve the best frame rate, is important to
understand how the OpenGL ES 2.0 code in a Native Client module interacts with
Chrome.

The Chrome and Native Client processes
--------------------------------------

Chrome is a multi-process browser. Each Chrome tab is a separate process that is
running an application with its own main thread (we'll call it the Chrome main
thread). When an application launches a Native Client module, the module runs in
a new, separate sandboxed process. The module's process has its own main thread
(the Native Client thread). The Chrome and Native Client processes communicate
with each other using Pepper API calls on their main threads.

When the Chrome main thread calls the Native Client thread (keyboard and mouse
callbacks, for example), the Chrome main thread will block. This means that
lengthy operations on the Native Client thread can steal cycles from Chrome, and
performing blocking operations on the Native Client thread can bring your app to
a standstill.

Native Client uses callback functions to synchronize the main threads of the
two processes. Only certain Pepper functions use callbacks; `SwapBuffers
</native-client/pepper_stable/c/struct_p_p_b___graphics3_d__1__0#a293c6941c0da084267ffba3954793497>`_
is one.

``SwapBuffers`` and its callback function
-----------------------------------------

``SwapBuffers`` is non-blocking; it is called from the Native Client thread and
returns immediately. When ``SwapBuffers`` is called, it runs asynchronously on
the Chrome main thread. It switches the graphics data buffers, handles any
needed compositing operations, and redraws the screen. When the screen update is
complete, the callback function that was included as one of ``SwapBuffer``'s
arguments will be called from the Chrome thread and executed on the Native
Client thread.

To create a rendering loop, your Native Client module should include a function
that does the rendering work and then executes ``SwapBuffers``, passing itself
as the ``SwapBuffer`` callback. If your rendering code is efficient and runs
quickly, this scheme will achieve the highest frame rate possible. The
documentation for ``SwapBuffers`` explains why this is optimal: because the
callback is executed only when the plugin's current state is actually on the
screen, this function provides a way to rate-limit animations. By waiting until
the image is on the screen before painting the next frame, you can ensure you're
not generating updates faster than the screen can be updated.

The following diagram illustrates the interaction between the Chrome and Native
Client processes. The application-specific rendering code runs in the function
called ``Draw`` on the Native Client thread. Blue down-arrows are blocking calls
from the main thread to Native Client, green up-arrows are non-blocking
``SwapBuffers`` calls from Native Client to the main thread. All OpenGL ES 2.0
calls are made from ``Draw`` in the Native Client thread.

.. image:: /images/3d-graphics-render-loop.png

SDK example ``graphics_3d``
---------------------------

The SDK example ``graphics_3d`` uses the function ``MainLoop`` (in
``hello_world.cc``) to create a rendering loop as described above. ``MainLoop``
calls ``Render`` to do the rendering work, and then invokes ``SwapBuffers``,
passing itself as the callback.

.. naclcode::

  void MainLoop(void* foo, int bar) {
    if (g_LoadCnt == 3) {
      InitProgram();
      g_LoadCnt++;
    }
    if (g_LoadCnt > 3) {
      Render();
      PP_CompletionCallback cc = PP_MakeCompletionCallback(MainLoop, 0);
      ppb_g3d_interface->SwapBuffers(g_context, cc);
    } else {
      PP_CompletionCallback cc = PP_MakeCompletionCallback(MainLoop, 0);
      ppb_core_interface->CallOnMainThread(0, cc, 0);
    }
  }

Managing the OpenGL ES 2.0 pipeline
===================================

OpenGL ES 2.0 commands do not run in the Chrome or Native Client processes. They
are passed into a FIFO queue in shared memory which is best understood as a `GPU
command buffer
<http://www.chromium.org/developers/design-documents/gpu-command-buffer>`_. The
command buffer is shared by a dedicated GPU process. By using a separate GPU
process, Chrome implements another layer of runtime security, vetting all OpenGL
ES 2.0 commands and their arguments before they are sent on to the
GPU. Buffering commands through the FIFO also speeds up your code, since each
OpenGL ES 2.0 call in your Native Client thread returns immediately, while the
processing may be delayed as the GPU works down the commands queued up in the
FIFO.

Before the screen is updated, all the intervening OpenGL ES 2.0 commands must be
processed by the GPU. Programmers often try to ensure this by using the
``glFlush`` and ``glFinish`` commands in their rendering code. In the case of
Native Client this is usually unnecessary. The ``SwapBuffers`` command does an
implicit flush, and the Chrome team is continually tweaking the GPU code to
consume the OpenGL ES 2.0 FIFO as fast as possible.

Sometimes a 3D application can write to the FIFO in a way that's difficult to
handle. The command pipeline may fill up and your code will have to wait for the
GPU to flush the FIFO. If this is the case, you may be able to add ``glFlush``
calls to speed up the flow of the OpenGL ES 2.0 command FIFO. Before you start
to add your own flushes, first try to determine if pipeline saturation is really
the problem by monitoring the rendering time per frame and looking for irregular
spikes that do not consistently fall on the same OpenGL ES 2.0 call. If you're
convinced the pipeline needs to be accelerated, insert ``glFlush`` calls in your
code before starting blocks of processing that do not generate OpenGL ES 2.0
commands. For example, issue a flush before you begin any multithreaded particle
work, so that the command buffer will be clear when you start doing OpenGL ES
2.0 calls again. Determining where and how often to call ``glFlush`` can be
tricky, you will need to experiment to find the sweet spot.

Rendering and inactive tabs
===========================

Users will often switch between tabs in a multi-tab browser. A well-behaved
application that's performing 3D rendering should pause any real-time processing
and yield cycles to other processes when its tab becomes inactive.

In Chrome, an inactive tab will continue to execute timed functions (such as
``setInterval`` and ``setTimeout``) but the timer interval will be automatically
overridden and limited to not less than one second while the tab is inactive. In
addition, any callback associated with a ``SwapBuffers`` call will not be sent
until the tab is active again. You may receive asynchronous callbacks from
functions other than ``SwapBuffers`` while a tab is inactive. Depending on the
design of your application, you might choose to handle them as they arrive, or
to queue them in a buffer and process them when the tab becomes active.

The time that passes while a tab is inactive can be considerable. If your main
thread pulse is based on the ``SwapBuffers`` callback, your app won't update
while a tab is inactive. A Native Client module should be able to detect and
respond to the state of the tab in which it's running. For example, when a tab
becomes inactive, you can set an atomic flag in the Native Client thread that
will skip the 3D rendering and ``SwapBuffers`` calls and continue to call the
main thread every 30 msec or so. This provides time to update features that
should still run in the background, like audio. It may also be helpful to call
``sched_yield`` or ``usleep`` on any worker threads to release resources and
cede cycles to the OS.

Handling tab activation from the main thread
--------------------------------------------

You can detect and respond to the activation or deactivation of a tab with
JavaScript on your hosting page. Add an EventListener for ``visibilitychange``
that sends a message to the Native Client module, as in this example:

.. naclcode::

  document.addEventListener('visibilitychange', function(){
    if (document.hidden) {
      // PostMessage to your Native Client module
      document.nacl_module.postMessage('INACTIVE');
    } else {
      // PostMessage to your Native Client module
      document.nacl_module.postMessage('ACTIVE');
    }

  }, false);

Handling tab activation from the Native Client thread
-----------------------------------------------------

You can also detect and respond to the activation or deactivation of a tab
directly from your Native Client module by including code in the function
``pp::Instance::DidChangeView``, which is called whenever a change in the
module's view occurs. The code can call ``ppb::View::IsPageVisible`` to
determine if the page is visible or not. The most common cause of invisible
pages is that the page is in a background tab.

Tips and best practices
=======================

Here are some suggestions for writing safe code and getting the maximum
performance with the Pepper 3D API.

Do's
----

* **Make sure to enable attrib 0.** OpenGL requires that you enable attrib 0,
  but OpenGL ES 2.0 does not. For example, you can define a vertex shader with 2
  attributes, numbered like this:

  .. naclcode::

    glBindAttribLocation(program, "positions", 1);
    glBindAttribLocation(program, "normals", 2);

  In this case the shader is not using attrib 0 and Chrome may have to perform
  some additional work if it is emulating OpenGL ES 2.0 on top of OpenGL. It's
  always more efficient to enable attrib 0, even if you do not use it.

* **Check how shaders compile.** Shaders can compile differently on different
  systems, which can result in ``glGetAttrib*`` functions returning different
  results. Be sure that the vertex attribute indices match the corresponding
  name each time you recompile a shader.

* **Update indices sparingly.** For security reasons, all indices must be
  validated. If you change indices, Native Client will validate them
  again. Structure your code so indices are not updated often.

* **Use a smaller plugin and let CSS scale it.** If you're running into fillrate
  issues, it may be beneficial to perform scaling via CSS. The size your plugin
  renders is determined by the width and height attributes of the ``<embed>``
  element for the module. The actual size displayed on the web page is
  controlled by the CSS styles applied to the element.

* **Avoid matrix-to-matrix conversions.** With some versions of Mac OS, there is
  a driver problem when compiling shaders. If you get compiler errors for matrix
  transforms, avoid matrix-to-matrix conversions. For instance, upres a vec3 to
  a vec4 before transforming it by a mat4, rather than converting the mat4 to a
  mat3.

Don'ts
------

* **Don't use client side buffers.** OpenGL ES 2.0 can use client side data with
  ``glVertexAttribPointer`` and ``glDrawElements``, but this is really slow. Try
  to avoid client side buffers. Use Vertex Buffer Objects (VBOs) instead.

* **Don't mix vertex data and index data.** By default, Pepper 3D binds buffers
  to a single point. You could create a buffer and bind it to both
  ``GL_ARRAY_BUFFER`` and ``GL_ELEMENT_ARRAY_BUFFER``, but that would be
  expensive overhead and it is not recommended.

* **Don't call glGet* or glCheck* during rendering.** This is normal
  advice for OpenGL programs, but is particularly important for 3D on
  Chrome. Calls to any OpenGL ES 2.0 function whose name begins with these
  strings blocks the Native Client thread. This includes ``glGetError``; avoid
  calling it in release builds.

* **Don't use fixed point (GL_FIXED) vertex attributes.** Fixed point
  attributes are not supported in OpenGL ES 2.0, so emulating them in OpenGL ES
  2.0 is slow. By default, ``GL_FIXED`` support is turned off in the Pepper 3D
  API.

* **Don't read data from the GPU.** Don't call ``glReadPixels``, as it is slow.

* **Don't update a small portion of a large buffer.** In the current OpenGL ES
  2.0 implementation when you update a portion of a buffer (with
  ``glSubBufferData`` for example) the entire buffer must be reprocessed. To
  avoid this problem, keep static and dynamic data in different buffers.

* **Don't call glDisable(GL_TEXTURE_2D).** This is an OpenGL ES 2.0
  error. Each time it is called, an error messages will appear in Chrome's
  ``about:gpu`` tab.

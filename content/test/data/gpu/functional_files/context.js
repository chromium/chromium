// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Global variables.
var gl_context;
var gl_renderer;

initializeWebGL = function(canvas, opt_attrs) {
  gl_context = null;
  // Try to grab the standard context.
  gl_context = canvas.getContext("webgl", opt_attrs);
  // If we don't have a GL context, give up now.
  if (!gl_context) {
    err = "Unable to initialize WebGL. Your browser may not support it.";
    if (window.domAutomationController) {
      console.log(err);
    } else {
      alert(err);
    }
  }
}

startWebGLContext = function(opt_attrs) {
  var canvas = document.getElementById("glcanvas");
  // Initialize the GL context.
  initializeWebGL(canvas, opt_attrs);

  // Only continue if WebGL is available and working.
  if (gl_context) {
    gl_context.clearColor(0.0, 0.0, 0.0, 1.0);
    gl_context.enable(gl_context.DEPTH_TEST);
    gl_context.depthFunc(gl_context.LEQUAL);
    gl_context.clearDepth(1);
    gl_context.clear(gl_context.COLOR_BUFFER_BIT |
                     gl_context.DEPTH_BUFFER_BIT);

    // Also fetch the unmasked GL_RENDERER string.
    var ext = gl_context.getExtension("WEBGL_debug_renderer_info");
    gl_renderer = gl_context.getParameter(ext.UNMASKED_RENDERER_WEBGL);
  }

  if (window.domAutomationController) {
    domAutomationController.send("FINISHED");
  }
}

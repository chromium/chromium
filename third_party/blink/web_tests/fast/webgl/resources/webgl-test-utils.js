// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

WebGLTestUtils = (function() {

/**
 * Wrapped logging function.
 * @param {string} msg The message to log.
 */
var log = function(msg) {
  if (window.console && window.console.log) {
    window.console.log(msg);
  }
};

/**
 * Wrapped logging function.
 * @param {string} msg The message to log.
 */
var error = function(msg) {
  if (window.console) {
    if (window.console.error) {
      window.console.error(msg);
    }
    else if (window.console.log) {
      window.console.log(msg);
    }
  }
};

/**
 * Turn off all logging.
 */
var loggingOff = function() {
  log = function() {};
  error = function() {};
};

/**
 * Converts a WebGL enum to a string
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} value The enum value.
 * @return {string} The enum as a string.
 */
var glEnumToString = function(gl, value) {
  for (var p in gl) {
    if (gl[p] == value) {
      return p;
    }
  }
  return "0x" + value.toString(16);
};

var lastError = "";

/**
 * Returns the last compiler/linker error.
 * @return {string} The last compiler/linker error.
 */
var getLastError = function() {
  return lastError;
};

/**
 * Whether a haystack ends with a needle.
 * @param {string} haystack String to search
 * @param {string} needle String to search for.
 * @param {boolean} True if haystack ends with needle.
 */
var endsWith = function(haystack, needle) {
  return haystack.substr(haystack.length - needle.length) === needle;
};

/**
 * Whether a haystack starts with a needle.
 * @param {string} haystack String to search
 * @param {string} needle String to search for.
 * @param {boolean} True if haystack starts with needle.
 */
var startsWith = function(haystack, needle) {
  return haystack.substr(0, needle.length) === needle;
};

/**
 * A vertex shader for a single texture.
 * @type {string}
 */
var simpleTextureVertexShader = [
  'attribute vec4 vPosition;',
  'attribute vec2 texCoord0;',
  'varying vec2 texCoord;',
  'void main() {',
  '    gl_Position = vPosition;',
  '    texCoord = texCoord0;',
  '}'].join('\n');

/**
 * A fragment shader for a single texture.
 * @type {string}
 */
var simpleTextureFragmentShader = [
  'precision mediump float;',
  'uniform sampler2D tex;',
  'varying vec2 texCoord;',
  'void main() {',
  '    gl_FragData[0] = texture2D(tex, texCoord);',
  '}'].join('\n');

/**
 * A vertex shader for a single texture.
 * @type {string}
 */
var simpleColorVertexShader = [
  'attribute vec4 vPosition;',
  'void main() {',
  '    gl_Position = vPosition;',
  '}'].join('\n');

/**
 * A fragment shader for a color.
 * @type {string}
 */
var simpleColorFragmentShader = [
  'precision mediump float;',
  'uniform vec4 u_color;',
  'void main() {',
  '    gl_FragData[0] = u_color;',
  '}'].join('\n');

/**
 * Creates a simple texture vertex shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @return {!WebGLShader}
 */
var setupSimpleTextureVertexShader = function(gl) {
    return loadShader(gl, simpleTextureVertexShader, gl.VERTEX_SHADER);
};

/**
 * Creates a simple texture fragment shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @return {!WebGLShader}
 */
var setupSimpleTextureFragmentShader = function(gl) {
    return loadShader(
        gl, simpleTextureFragmentShader, gl.FRAGMENT_SHADER);
};

/**
 * Creates a program, attaches shaders, binds attrib locations, links the
 * program and calls useProgram.
 * @param {!Array.<!WebGLShader>} shaders The shaders to attach .
 * @param {!Array.<string>} opt_attribs The attribs names.
 * @param {!Array.<number>} opt_locations The locations for the attribs.
 */
var setupProgram = function(gl, shaders, opt_attribs, opt_locations) {
  var realShaders = [];
  var program = gl.createProgram();
  for (var ii = 0; ii < shaders.length; ++ii) {
    var shader = shaders[ii];
    if (typeof shader == 'string') {
      var element = document.getElementById(shader);
      if (element) {
        shader = loadShaderFromScript(gl, shader);
      } else {
        shader = loadShader(gl, shader, ii ? gl.FRAGMENT_SHADER : gl.VERTEX_SHADER);
      }
    }
    gl.attachShader(program, shader);
  }
  if (opt_attribs) {
    for (var ii = 0; ii < opt_attribs.length; ++ii) {
      gl.bindAttribLocation(
          program,
          opt_locations ? opt_locations[ii] : ii,
          opt_attribs[ii]);
    }
  }
  gl.linkProgram(program);

  // Check the link status
  var linked = gl.getProgramParameter(program, gl.LINK_STATUS);
  if (!linked) {
      // something went wrong with the link
      lastError = gl.getProgramInfoLog (program);
      error("Error in program linking:" + lastError);

      gl.deleteProgram(program);
      return null;
  }

  gl.useProgram(program);
  return program;
};

/**
 * Creates a simple texture program.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {WebGLProgram}
 */
var setupSimpleTextureProgram = function(
    gl, opt_positionLocation, opt_texcoordLocation) {
  opt_positionLocation = opt_positionLocation || 0;
  opt_texcoordLocation = opt_texcoordLocation || 1;
  var vs = setupSimpleTextureVertexShader(gl);
  var fs = setupSimpleTextureFragmentShader(gl);
  if (!vs || !fs) {
    return null;
  }
  var program = setupProgram(
      gl,
      [vs, fs],
      ['vPosition', 'texCoord0'],
      [opt_positionLocation, opt_texcoordLocation]);
  if (!program) {
    gl.deleteShader(fs);
    gl.deleteShader(vs);
  }
  gl.useProgram(program);
  return program;
};

/**
 * Creates buffers for a textured unit quad and attaches them to vertex attribs.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {!Array.<WebGLBuffer>} The buffer objects that were
 *      created.
 */
var setupUnitQuad = function(gl, opt_positionLocation, opt_texcoordLocation) {
  opt_positionLocation = opt_positionLocation || 0;
  opt_texcoordLocation = opt_texcoordLocation || 1;
  var objects = [];

  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
       1.0,  1.0, 0.0,
      -1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0,  1.0, 0.0,
      -1.0, -1.0, 0.0,
       1.0, -1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(opt_positionLocation);
  gl.vertexAttribPointer(opt_positionLocation, 3, gl.FLOAT, false, 0, 0);
  objects.push(vertexObject);

  var vertexObject = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, vertexObject);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([
      1.0, 1.0,
      0.0, 1.0,
      0.0, 0.0,
      1.0, 1.0,
      0.0, 0.0,
      1.0, 0.0]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(opt_texcoordLocation);
  gl.vertexAttribPointer(opt_texcoordLocation, 2, gl.FLOAT, false, 0, 0);
  objects.push(vertexObject);
  return objects;
};

/**
 * Creates a program and buffers for rendering a textured quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @param {number} opt_texcoordLocation The attrib location for texture coords.
 * @return {!WebGLProgram}
 */
var setupTexturedQuad = function(
    gl, opt_positionLocation, opt_texcoordLocation) {
  var program = setupSimpleTextureProgram(
      gl, opt_positionLocation, opt_texcoordLocation);
  setupUnitQuad(gl, opt_positionLocation, opt_texcoordLocation);
  return program;
};

/**
 * Creates a program and buffers for rendering a color quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} opt_positionLocation The attrib location for position.
 * @return {!WebGLProgram}
 */
var setupColorQuad = function(gl, opt_positionLocation) {
  opt_positionLocation = opt_positionLocation || 0;
  var program = wtu.setupProgram(
      gl,
      [simpleColorVertexShader, simpleColorFragmentShader],
      ['vPosition'],
      [opt_positionLocation]);
  setupUnitQuad(gl, opt_positionLocation);
  return program;
};

/**
 * Creates a unit quad with only positions of a given rez
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} gridRez The resolution of the mesh grid.
 * @param {number} opt_positionLocation The attrib location for position.
 */
var setupQuad = function (
    gl, gridRes, opt_positionLocation, opt_flipOddTriangles) {
  var positionLocation = opt_positionLocation || 0;
  var objects = [];

  var vertsAcross = gridRes + 1;
  var numVerts = vertsAcross * vertsAcross;
  var positions = new Float32Array(numVerts * 3);
  var indices = new Uint16Array(6 * gridRes * gridRes);

  var poffset = 0;

  for (var yy = 0; yy <= gridRes; ++yy) {
    for (var xx = 0; xx <= gridRes; ++xx) {
      positions[poffset + 0] = -1 + 2 * xx / gridRes;
      positions[poffset + 1] = -1 + 2 * yy / gridRes;
      positions[poffset + 2] = 0;

      poffset += 3;
    }
  }

  var tbase = 0;
  for (var yy = 0; yy < gridRes; ++yy) {
    var index = yy * vertsAcross;
    for (var xx = 0; xx < gridRes; ++xx) {
      indices[tbase + 0] = index + 0;
      indices[tbase + 1] = index + 1;
      indices[tbase + 2] = index + vertsAcross;
      indices[tbase + 3] = index + vertsAcross;
      indices[tbase + 4] = index + 1;
      indices[tbase + 5] = index + vertsAcross + 1;

      if (opt_flipOddTriangles) {
        indices[tbase + 4] = index + vertsAcross + 1;
        indices[tbase + 5] = index + 1;
      }

      index += 1;
      tbase += 6;
    }
  }

  var buf = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, buf);
  gl.bufferData(gl.ARRAY_BUFFER, positions, gl.STATIC_DRAW);
  gl.enableVertexAttribArray(positionLocation);
  gl.vertexAttribPointer(positionLocation, 3, gl.FLOAT, false, 0, 0);
  objects.push(buf);

  var buf = gl.createBuffer();
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, buf);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indices, gl.STATIC_DRAW);
  objects.push(buf);

  return objects;
};

/**
 * Fills the given texture with a solid color
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!WebGLTexture} tex The texture to fill.
 * @param {number} width The width of the texture to create.
 * @param {number} height The height of the texture to create.
 * @param {!Array.<number>} color The color to fill with. A 4 element array
 *        where each element is in the range 0 to 255.
 * @param {number} opt_level The level of the texture to fill. Default = 0.
 */
var fillTexture = function(gl, tex, width, height, color, opt_level) {
  opt_level = opt_level || 0;
  var numPixels = width * height;
  var size = numPixels * 4;
  var buf = new Uint8Array(size);
  for (var ii = 0; ii < numPixels; ++ii) {
    var off = ii * 4;
    buf[off + 0] = color[0];
    buf[off + 1] = color[1];
    buf[off + 2] = color[2];
    buf[off + 3] = color[3];
  }
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(
      gl.TEXTURE_2D, opt_level, gl.RGBA, width, height, 0,
      gl.RGBA, gl.UNSIGNED_BYTE, buf);
  };

/**
 * Creates a textures and fills it with a solid color
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} width The width of the texture to create.
 * @param {number} height The height of the texture to create.
 * @param {!Array.<number>} color The color to fill with. A 4 element array
 *        where each element is in the range 0 to 255.
 * @return {!WebGLTexture}
 */
var createColoredTexture = function(gl, width, height, color) {
  var tex = gl.createTexture();
  fillTexture(gl, tex, width, height, color);
  return tex;
};

var ubyteToFloat = function(c) {
  return c / 255;
};

/**
 * Draws a previously setup quad in the given color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} color The color to draw with. A 4
 *        element array where each element is in the range 0 to
 *        1.
 */
var drawFloatColorQuad = function(gl, color) {
  var program = gl.getParameter(gl.CURRENT_PROGRAM);
  var colorLocation = gl.getUniformLocation(program, "u_color");
  gl.uniform4fv(colorLocation, color);
  gl.drawArrays(gl.TRIANGLES, 0, 6);
};


/**
 * Draws a previously setup quad in the given color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} color The color to draw with. A 4
 *        element array where each element is in the range 0 to
 *        255.
 */
var drawUByteColorQuad = function(gl, color) {
  var floatColor = [];
  for (var ii = 0; ii < color.length; ++ii) {
    floatColor[ii] = ubyteToFloat(color[ii]);
  }
  drawFloatColorQuad(gl, floatColor);
};

/**
 * Draws a previously setup quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} opt_color The color to fill clear with before
 *        drawing. A 4 element array where each element is in the range 0 to
 *        255. Default [255, 255, 255, 255]
 */
var drawQuad = function(gl, opt_color) {
  opt_color = opt_color || [255, 255, 255, 255];
  gl.clearColor(
      opt_color[0] / 255,
      opt_color[1] / 255,
      opt_color[2] / 255,
      opt_color[3] / 255);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.drawArrays(gl.TRIANGLES, 0, 6);
};

/**
 * Draws a previously setup quad.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} gridRes Resolution of grid.
 * @param {!Array.<number>} opt_color The color to fill clear with before
 *        drawing. A 4 element array where each element is in the range 0 to
 *        255. Default [255, 255, 255, 255]
 */
var drawIndexedQuad = function(gl, gridRes, opt_color) {
  opt_color = opt_color || [255, 255, 255, 255];
  gl.clearColor(
      opt_color[0] / 255,
      opt_color[1] / 255,
      opt_color[2] / 255,
      opt_color[3] / 255);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
  gl.drawElements(gl.TRIANGLES, gridRes * 6, gl.UNSIGNED_SHORT, 0);
};

/**
 * Checks that a portion of a canvas is 1 color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} x left corner of region to check.
 * @param {number} y bottom corner of region to check.
 * @param {number} width width of region to check.
 * @param {number} height width of region to check.
 * @param {!Array.<number>} color The color to fill clear with before drawing. A
 *        4 element array where each element is in the range 0 to 255.
 * @param {string} msg Message to associate with success. Eg ("should be red").
 * @param {number} errorRange Optional. Acceptable error in
 *        color checking. 0 by default.
 */
var checkCanvasRect = function(gl, x, y, width, height, color, msg, errorRange) {
  errorRange = errorRange || 0;
  var buf = new Uint8Array(width * height * 4);
  gl.readPixels(x, y, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
  for (var i = 0; i < width * height; ++i) {
    var offset = i * 4;
    for (var j = 0; j < color.length; ++j) {
      if (Math.abs(buf[offset + j] - color[j]) > errorRange) {
        testFailed(msg);
        var was = buf[offset + 0].toString();
        for (j = 1; j < color.length; ++j) {
          was += "," + buf[offset + j];
        }
        debug('at (' + (i % width) + ', ' + Math.floor(i / width) +
              ') expected: ' + color + ' was ' + was);
        return;
      }
    }
  }
  testPassed(msg);
};

/**
 * Checks that an entire canvas is 1 color.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!Array.<number>} color The color to fill clear with before drawing. A
 *        4 element array where each element is in the range 0 to 255.
 * @param {string} msg Message to associate with success. Eg ("should be red").
 * @param {number} errorRange Optional. Acceptable error in
 *        color checking. 0 by default.
 */
var checkCanvas = function(gl, color, msg, errorRange) {
  checkCanvasRect(gl, 0, 0, gl.canvas.width, gl.canvas.height, color, msg, errorRange);
};

/**
 * Loads a texture, calls callback when finished.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} url URL of image to load
 * @param {function(!Image): void} callback Function that gets called after
 *        image has loaded
 * @return {!WebGLTexture} The created texture.
 */
var loadTexture = function(gl, url, callback) {
    var texture = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, texture);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
    var image = new Image();
    image.onload = function() {
        gl.bindTexture(gl.TEXTURE_2D, texture);
        gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
        gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, image);
        callback(image);
    };
    image.src = url;
    return texture;
};

/**
 * Creates a webgl context.
 * @param {!Canvas} opt_canvas The canvas tag to get context from. If one is not
 *     passed in one will be created.
 * @return {!WebGLContext} The created context.
 */
var create3DContext = function(opt_canvas, opt_attributes) {
  opt_canvas = opt_canvas || document.createElement("canvas");
  if (typeof opt_canvas == 'string') {
    opt_canvas = document.getElementById(opt_canvas);
  }
  var context = null;
  var names = ["webgl", "webgl"];
  for (var i = 0; i < names.length; ++i) {
    try {
      context = opt_canvas.getContext(names[i], opt_attributes);
    } catch (e) {
    }
    if (context) {
      break;
    }
  }
  if (!context) {
    testFailed("Unable to fetch WebGL rendering context for Canvas");
  }
  return context;
}

/**
 * Gets a GLError value as a string.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} err The webgl error as retrieved from gl.getError().
 * @return {string} the error as a string.
 */
var getGLErrorAsString = function(gl, err) {
  if (err === gl.NO_ERROR) {
    return "NO_ERROR";
  }
  for (var name in gl) {
    if (gl[name] === err) {
      return name;
    }
  }
  return err.toString();
};

/**
 * Wraps a WebGL function with a function that throws an exception if there is
 * an error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} fname Name of function to wrap.
 * @return {function} The wrapped function.
 */
var createGLErrorWrapper = function(context, fname) {
  return function() {
    var rv = context[fname].apply(context, arguments);
    var err = context.getError();
    if (err != 0)
      throw "GL error " + getGLErrorAsString(err) + " in " + fname;
    return rv;
  };
};

/**
 * Creates a WebGL context where all functions are wrapped to throw an exception
 * if there is an error.
 * @param {!Canvas} canvas The HTML canvas to get a context from.
 * @return {!Object} The wrapped context.
 */
function create3DContextWithWrapperThatThrowsOnGLError(canvas) {
  var context = create3DContext(canvas);
  var wrap = {};
  for (var i in context) {
    try {
      if (typeof context[i] == 'function') {
        wrap[i] = createGLErrorWrapper(context, i);
      } else {
        wrap[i] = context[i];
      }
    } catch (e) {
      error("createContextWrapperThatThrowsOnGLError: Error accessing " + i);
    }
  }
  wrap.getError = function() {
      return context.getError();
  };
  return wrap;
};

/**
 * Tests that an evaluated expression generates a specific GL error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} glError The expected gl error.
 * @param {string} evalSTr The string to evaluate.
 */
var shouldGenerateGLError = function(gl, glError, evalStr) {
  var exception;
  try {
    eval(evalStr);
  } catch (e) {
    exception = e;
  }
  if (exception) {
    testFailed(evalStr + " threw exception " + exception);
  } else {
    var err = gl.getError();
    if (err != glError) {
      testFailed(evalStr + " expected: " + getGLErrorAsString(gl, glError) + ". Was " + getGLErrorAsString(gl, err) + ".");
    } else {
      testPassed(evalStr + " was expected value: " + getGLErrorAsString(gl, glError) + ".");
    }
  }
};

/**
 * Tests that the first error GL returns is the specified error.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {number} glError The expected gl error.
 * @param {string} opt_msg
 */
var glErrorShouldBe = function(gl, glError, opt_msg) {
  opt_msg = opt_msg || "";
  var err = gl.getError();
  if (err != glError) {
    testFailed("getError expected: " + getGLErrorAsString(gl, glError) +
               ". Was " + getGLErrorAsString(gl, err) + " : " + opt_msg);
  } else {
    testPassed("getError was expected value: " +
                getGLErrorAsString(gl, glError) + " : " + opt_msg);
  }
};

/**
 * Links a WebGL program, throws if there are errors.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {!WebGLProgram} program The WebGLProgram to link.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 */
var linkProgram = function(gl, program, opt_errorCallback) {
  // Link the program
  gl.linkProgram(program);

  // Check the link status
  var linked = gl.getProgramParameter(program, gl.LINK_STATUS);
  if (!linked) {
    // something went wrong with the link
    var error = gl.getProgramInfoLog (program);

    testFailed("Error in program linking:" + error);

    gl.deleteProgram(program);
  }
};

/**
 * Sets up WebGL with shaders.
 * @param {string} canvasName The id of the canvas.
 * @param {string} vshader The id of the script tag that contains the vertex
 *     shader source.
 * @param {string} fshader The id of the script tag that contains the fragment
 *     shader source.
 * @param {!Array.<string>} attribs An array of attrib names used to bind
 *     attribs to the ordinal of the name in this array.
 * @param {!Array.<number>} opt_clearColor The color to cla
 * @return {!WebGLContext} The created WebGLContext.
 */
var setupWebGLWithShaders = function(
   canvasName, vshader, fshader, attribs) {
  var canvas = document.getElementById(canvasName);
  var gl = create3DContext(canvas);
  if (!gl) {
    testFailed("No WebGL context found");
  }

  // create our shaders
  var vertexShader = loadShaderFromScript(gl, vshader);
  var fragmentShader = loadShaderFromScript(gl, fshader);

  if (!vertexShader || !fragmentShader) {
    return null;
  }

  // Create the program object
  program = gl.createProgram();

  if (!program) {
    return null;
  }

  // Attach our two shaders to the program
  gl.attachShader (program, vertexShader);
  gl.attachShader (program, fragmentShader);

  // Bind attributes
  for (var i in attribs) {
    gl.bindAttribLocation (program, i, attribs[i]);
  }

  linkProgram(gl, program);

  gl.useProgram(program);

  gl.clearColor(0,0,0,1);
  gl.clearDepth(1);

  gl.enable(gl.DEPTH_TEST);
  gl.enable(gl.BLEND);
  gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

  gl.program = program;
  return gl;
};

/**
 * Loads text from an external file. This function is synchronous.
 * @param {string} url The url of the external file.
 * @param {!function(bool, string): void} callback that is sent a bool for
 *     success and the string.
 */
var loadTextFileAsync = function(url, callback) {
  log ("loading: " + url);
  var error = 'loadTextFileSynchronous failed to load url "' + url + '"';
  var request;
  if (window.XMLHttpRequest) {
    request = new XMLHttpRequest();
    if (request.overrideMimeType) {
      request.overrideMimeType('text/plain');
    }
  } else {
    throw 'XMLHttpRequest is disabled';
  }
  try {
    request.open('GET', url, true);
    request.onreadystatechange = function() {
      if (request.readyState == 4) {
        var text = '';
        // HTTP reports success with a 200 status. The file protocol reports
        // success with zero. HTTP does not use zero as a status code (they
        // start at 100).
        // https://developer.mozilla.org/En/Using_XMLHttpRequest
        var success = request.status == 200 || request.status == 0;
        if (success) {
          text = request.responseText;
        }
        log("loaded: " + url);
        callback(success, text);
      }
    };
    request.send(null);
  } catch (e) {
    log("failed to load: " + url);
    callback(false, '');
  }
};

/**
 * Recursively loads a file as a list. Each line is parsed for a relative
 * path. If the file ends in .txt the contents of that file is inserted in
 * the list.
 *
 * @param {string} url The url of the external file.
 * @param {!function(bool, Array<string>): void} callback that is sent a bool
 *     for success and the array of strings.
 */
var getFileListAsync = function(url, callback) {
  var files = [];

  var getFileListImpl = function(url, callback) {
    var files = [];
    if (url.substr(url.length - 4) == '.txt') {
      loadTextFileAsync(url, function() {
        return function(success, text) {
          if (!success) {
            callback(false, '');
            return;
          }
          var lines = text.split('\n');
          var prefix = '';
          var lastSlash = url.lastIndexOf('/');
          if (lastSlash >= 0) {
            prefix = url.substr(0, lastSlash + 1);
          }
          var fail = false;
          var count = 1;
          var index = 0;
          for (var ii = 0; ii < lines.length; ++ii) {
            var str = lines[ii].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
            if (str.length > 4 &&
                str[0] != '#' &&
                str[0] != ";" &&
                str.substr(0, 2) != "//") {
              var names = str.split(/ +/);
              new_url = prefix + str;
              if (names.length == 1) {
                new_url = prefix + str;
                ++count;
                getFileListImpl(new_url, function(index) {
                  return function(success, new_files) {
                    log("got files: " + new_files.length);
                    if (success) {
                      files[index] = new_files;
                    }
                    finish(success);
                  };
                }(index++));
              } else {
                var s = "";
                var p = "";
                for (var jj = 0; jj < names.length; ++jj) {
                  s += p + prefix + names[jj];
                  p = " ";
                }
                files[index++] = s;
              }
            }
          }
          finish(true);

          function finish(success) {
            if (!success) {
              fail = true;
            }
            --count;
            log("count: " + count);
            if (!count) {
              callback(!fail, files);
            }
          }
        }
      }());

    } else {
      files.push(url);
      callback(true, files);
    }
  };

  getFileListImpl(url, function(success, files) {
    // flatten
    var flat = [];
    flatten(files);
    function flatten(files) {
      for (var ii = 0; ii < files.length; ++ii) {
        var value = files[ii];
        if (typeof(value) == "string") {
          flat.push(value);
        } else {
          flatten(value);
        }
      }
    }
    callback(success, flat);
  });
};

/**
 * Gets a file from a file/URL
 * @param {string} file the URL of the file to get.
 * @return {string} The contents of the file.
 */
var readFile = function(file) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", file, false);
  xhr.send();
  return xhr.responseText.replace(/\r/g, "");
};

var readFileList = function(url) {
  var files = [];
  if (url.substr(url.length - 4) == '.txt') {
    var lines = readFile(url).split('\n');
    var prefix = '';
    var lastSlash = url.lastIndexOf('/');
    if (lastSlash >= 0) {
      prefix = url.substr(0, lastSlash + 1);
    }
    for (var ii = 0; ii < lines.length; ++ii) {
      var str = lines[ii].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
      if (str.length > 4 &&
          str[0] != '#' &&
          str[0] != ";" &&
          str.substr(0, 2) != "//") {
        var names = str.split(/ +/);
        if (names.length == 1) {
          new_url = prefix + str;
          files = files.concat(readFileList(new_url));
        } else {
          var s = "";
          var p = "";
          for (var jj = 0; jj < names.length; ++jj) {
            s += p + prefix + names[jj];
            p = " ";
          }
          files.push(s);
        }
      }
    }
  } else {
    files.push(url);
  }
  return files;
};

/**
 * Loads a shader.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} shaderSource The shader source.
 * @param {number} shaderType The type of shader. 
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLShader} The created shader.
 */
var loadShader = function(gl, shaderSource, shaderType, opt_errorCallback) {
  var errFn = opt_errorCallback || error;
  // Create the shader object
  var shader = gl.createShader(shaderType);
  if (shader == null) {
    errFn("*** Error: unable to create shader '"+shaderSource+"'");
    return null;
  }

  // Load the shader source
  gl.shaderSource(shader, shaderSource);
  var err = gl.getError();
  if (err != gl.NO_ERROR) {
    errFn("*** Error loading shader '" + shader + "':" + glEnumToString(gl, err));
    return null;
  }

  // Compile the shader
  gl.compileShader(shader);

  // Check the compile status
  var compiled = gl.getShaderParameter(shader, gl.COMPILE_STATUS);
  if (!compiled) {
    // Something went wrong during compilation; get the error
    lastError = gl.getShaderInfoLog(shader);
    errFn("*** Error compiling shader '" + shader + "':" + lastError);
    gl.deleteShader(shader);
    return null;
  }

  return shader;
}

/**
 * Loads a shader from a URL.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {file} file The URL of the shader source.
 * @param {number} type The type of shader.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLShader} The created shader.
 */
var loadShaderFromFile = function(gl, file, type, opt_errorCallback) {
  var shaderSource = readFile(file);
  return loadShader(gl, shaderSource, type, opt_errorCallback);
};

/**
 * Loads a shader from a script tag.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} scriptId The id of the script tag.
 * @param {number} opt_shaderType The type of shader. If not passed in it will
 *     be derived from the type of the script tag.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLShader} The created shader.
 */
var loadShaderFromScript = function(
    gl, scriptId, opt_shaderType, opt_errorCallback) {
  var shaderSource = "";
  var shaderType;
  var shaderScript = document.getElementById(scriptId);
  if (!shaderScript) {
    throw("*** Error: unknown script element" + scriptId);
  }
  shaderSource = shaderScript.text;

  if (!opt_shaderType) {
    if (shaderScript.type == "x-shader/x-vertex") {
      shaderType = gl.VERTEX_SHADER;
    } else if (shaderScript.type == "x-shader/x-fragment") {
      shaderType = gl.FRAGMENT_SHADER;
    } else if (shaderType != gl.VERTEX_SHADER && shaderType != gl.FRAGMENT_SHADER) {
      throw("*** Error: unknown shader type");
      return null;
    }
  }

  return loadShader(
      gl, shaderSource, opt_shaderType ? opt_shaderType : shaderType,
      opt_errorCallback);
};

var loadStandardProgram = function(gl) {
  var program = gl.createProgram();
  gl.attachShader(program, loadStandardVertexShader(gl));
  gl.attachShader(program, loadStandardFragmentShader(gl));
  linkProgram(gl, program);
  return program;
};

/**
 * Loads shaders from files, creates a program, attaches the shaders and links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexShaderPath The URL of the vertex shader.
 * @param {string} fragmentShaderPath The URL of the fragment shader.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLProgram} The created program.
 */
var loadProgramFromFile = function(
    gl, vertexShaderPath, fragmentShaderPath, opt_errorCallback) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShaderFromFile(
          gl, vertexShaderPath, gl.VERTEX_SHADER, opt_errorCallback));
  gl.attachShader(
      program,
      loadShaderFromFile(
          gl, fragmentShaderPath, gl.FRAGMENT_SHADER, opt_errorCallback));
  linkProgram(gl, program, opt_errorCallback);
  return program;
};

/**
 * Loads shaders from script tags, creates a program, attaches the shaders and
 * links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexScriptId The id of the script tag that contains the
 *        vertex shader.
 * @param {string} fragmentScriptId The id of the script tag that contains the
 *        fragment shader.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLProgram} The created program.
 */
var loadProgramFromScript = function loadProgramFromScript(
    gl, vertexScriptId, fragmentScriptId, opt_errorCallback) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShaderFromScript(
          gl, vertexScriptId, gl.VERTEX_SHADER, opt_errorCallback));
  gl.attachShader(
      program,
      loadShaderFromScript(
          gl, fragmentScriptId,  gl.FRAGMENT_SHADER, opt_errorCallback));
  linkProgram(gl, program, opt_errorCallback);
  return program;
};

/**
 * Loads shaders from source, creates a program, attaches the shaders and
 * links.
 * @param {!WebGLContext} gl The WebGLContext to use.
 * @param {string} vertexShader The vertex shader.
 * @param {string} fragmentShader The fragment shader.
 * @param {function(string): void) opt_errorCallback callback for errors. 
 * @return {!WebGLProgram} The created program.
 */
var loadProgram = function(
    gl, vertexShader, fragmentShader, opt_errorCallback) {
  var program = gl.createProgram();
  gl.attachShader(
      program,
      loadShader(
          gl, vertexShader, gl.VERTEX_SHADER, opt_errorCallback));
  gl.attachShader(
      program,
      loadShader(
          gl, fragmentShader, gl.FRAGMENT_SHADER, opt_errorCallback));
  linkProgram(gl, program, opt_errorCallback);
  return program;
};

var basePath;
var getBasePath = function() {
  if (!basePath) {
    var expectedBase = "webgl-test-utils.js";
    var scripts = document.getElementsByTagName('script');
    for (var script, i = 0; script = scripts[i]; i++) {
      var src = script.src;
      var l = src.length;
      if (src.substr(l - expectedBase.length) == expectedBase) {
        basePath = src.substr(0, l - expectedBase.length);
      }
    }
  }
  return basePath;
};

var loadStandardVertexShader = function(gl) {
  return loadShaderFromFile(
      gl, getBasePath() + "vertexShader.vert", gl.VERTEX_SHADER);
};

var loadStandardFragmentShader = function(gl) {
  return loadShaderFromFile(
      gl, getBasePath() + "fragmentShader.frag", gl.FRAGMENT_SHADER);
};

/**
 * Loads an image asynchronously.
 * @param {string} url URL of image to load.
 * @param {!function(!Element): void} callback Function to call
 *     with loaded image.
 */
var loadImageAsync = function(url, callback) {
  var img = document.createElement('img');
  img.onload = function() {
    callback(img);
  };
  img.src = url;
};

/**
 * Loads an array of images.
 * @param {!Array.<string>} urls URLs of images to load.
 * @param {!function(!{string, img}): void} callback. Callback
 *     that gets passed map of urls to img tags.
 */
var loadImagesAsync = function(urls, callback) {
  var count = 1;
  var images = { };
  function countDown() {
    --count;
    if (count == 0) {
      callback(images);
    }
  }
  function imageLoaded(url) {
    return function(img) {
      images[url] = img;
      countDown();
    }
  }
  for (var ii = 0; ii < urls.length; ++ii) {
    ++count;
    loadImageAsync(urls[ii], imageLoaded(urls[ii]));
  }
  countDown();
};

var getUrlArguments = function() {
  var args = {};
  try {
    var s = window.location.href;
    var q = s.indexOf("?");
    var e = s.indexOf("#");
    if (e < 0) {
      e = s.length;
    }
    var query = s.substring(q + 1, e);
    var pairs = query.split("&");
    for (var ii = 0; ii < pairs.length; ++ii) {
      var keyValue = pairs[ii].split("=");
      var key = keyValue[0];
      var value = decodeURIComponent(keyValue[1]);
      args[key] = value;
    }
  } catch (e) {
    throw "could not parse url";
  }
  return args;
};

// Add your prefix here.
var browserPrefixes = [
  "",
  "MOZ_",
  "OP_",
  "WEBKIT_"
];

/**
 * Given an extension name like WEBGL_compressed_texture_s3tc
 * returns the name of the supported version extension, like
 * WEBKIT_WEBGL_compressed_teture_s3tc
 * @param {string} name Name of extension to look for
 * @return {string} name of extension found or undefined if not
 *     found.
 */
var getSupportedExtensionWithKnownPrefixes = function(gl, name) {
  var supported = gl.getSupportedExtensions();
  for (var ii = 0; ii < browserPrefixes.length; ++ii) {
    var prefixedName = browserPrefixes[ii] + name;
    if (supported.indexOf(prefixedName) >= 0) {
      return prefixedName;
    }
  }
};

/**
 * Given an extension name like WEBGL_compressed_texture_s3tc
 * returns the supported version extension, like
 * WEBKIT_WEBGL_compressed_teture_s3tc
 * @param {string} name Name of extension to look for
 * @return {WebGLExtension} The extension or undefined if not
 *     found.
 */
var getExtensionWithKnownPrefixes = function(gl, name) {
  for (var ii = 0; ii < browserPrefixes.length; ++ii) {
    var prefixedName = browserPrefixes[ii] + name;
    var ext = gl.getExtension(prefixedName);
    if (ext) {
      return ext;
    }
  }
};

/**
 * Provides requestAnimationFrame in a cross browser way.
 */
var requestAnimFrameImpl_;

var requestAnimFrame = function(callback, element) {
  if (!requestAnimFrameImpl_) {
    requestAnimFrameImpl_ = function() {
      var functionNames = [
        "requestAnimationFrame",
        "webkitRequestAnimationFrame",
        "mozRequestAnimationFrame",
        "oRequestAnimationFrame",
        "msRequestAnimationFrame"
      ];
      for (var jj = 0; jj < functionNames.length; ++jj) {
        var functionName = functionNames[jj];
        if (window[functionName]) {
          return function(name) {
            return function(callback, element) {
              return window[name].call(window, callback, element);
            };
          }(functionName);
        }
      }
      return function(callback, element) {
           return window.setTimeout(callback, 1000 / 70);
        };
    }();
  }

  return requestAnimFrameImpl_(callback, element);
};

/**
 * Provides cancelAnimationFrame in a cross browser way.
 */
var cancelAnimFrame = (function() {
  return window.cancelAnimationFrame ||
         window.webkitCancelAnimationFrame ||
         window.mozCancelAnimationFrame ||
         window.oCancelAnimationFrame ||
         window.msCancelAnimationFrame ||
         window.clearTimeout;
})();

var waitForComposite = function(frames, callback) {
  var countDown = function() {
    if (frames == 0) {
      callback();
    } else {
      --frames;
      requestAnimFrame(countDown);
    }
  };
  countDown();
};

return {
  cancelAnimFrame: cancelAnimFrame,
  create3DContext: create3DContext,
  create3DContextWithWrapperThatThrowsOnGLError:
    create3DContextWithWrapperThatThrowsOnGLError,
  checkCanvas: checkCanvas,
  checkCanvasRect: checkCanvasRect,
  createColoredTexture: createColoredTexture,
  drawQuad: drawQuad,
  drawIndexedQuad: drawIndexedQuad,
  drawUByteColorQuad: drawUByteColorQuad,
  drawFloatColorQuad: drawFloatColorQuad,
  endsWith: endsWith,
  getExtensionWithKnownPrefixes: getExtensionWithKnownPrefixes,
  getFileListAsync: getFileListAsync,
  getLastError: getLastError,
  getSupportedExtensionWithKnownPrefixes: getSupportedExtensionWithKnownPrefixes,
  getUrlArguments: getUrlArguments,
  glEnumToString: glEnumToString,
  glErrorShouldBe: glErrorShouldBe,
  fillTexture: fillTexture,
  loadImageAsync: loadImageAsync,
  loadImagesAsync: loadImagesAsync,
  loadProgram: loadProgram,
  loadProgramFromFile: loadProgramFromFile,
  loadProgramFromScript: loadProgramFromScript,
  loadShader: loadShader,
  loadShaderFromFile: loadShaderFromFile,
  loadShaderFromScript: loadShaderFromScript,
  loadStandardProgram: loadStandardProgram,
  loadStandardVertexShader: loadStandardVertexShader,
  loadStandardFragmentShader: loadStandardFragmentShader,
  loadTextFileAsync: loadTextFileAsync,
  loadTexture: loadTexture,
  log: log,
  loggingOff: loggingOff,
  error: error,
  setupColorQuad: setupColorQuad,
  setupProgram: setupProgram,
  setupQuad: setupQuad,
  setupSimpleTextureFragmentShader: setupSimpleTextureFragmentShader,
  setupSimpleTextureProgram: setupSimpleTextureProgram,
  setupSimpleTextureVertexShader: setupSimpleTextureVertexShader,
  setupTexturedQuad: setupTexturedQuad,
  setupUnitQuad: setupUnitQuad,
  setupWebGLWithShaders: setupWebGLWithShaders,
  startsWith: startsWith,
  shouldGenerateGLError: shouldGenerateGLError,
  readFile: readFile,
  readFileList: readFileList,
  requestAnimFrame: requestAnimFrame,
  waitForComposite: waitForComposite,

  none: false
};

}());



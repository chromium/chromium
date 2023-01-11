var wtu = WebGLTestUtils;
var tiu = TexImageUtils;
var gl = null;
var internalFormat = "RGBA";
var pixelFormat = "RGBA";
var pixelType = "UNSIGNED_BYTE";
var pixelsBuffer = [];
var resizeQualities = ["pixelated", "low", "medium", "high"];

function runOneIteration(useTexSubImage2D, bindingTarget, program, bitmap,
                         flipY, premultiplyAlpha, retVal, colorSpace,
                         testOptions)
{
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    // Enable writes to the RGBA channels
    gl.colorMask(1, 1, 1, 1);
    var texture = gl.createTexture();
    // Bind the texture to texture unit 0
    gl.bindTexture(bindingTarget, texture);
    // Set up texture parameters
    gl.texParameteri(bindingTarget, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
    gl.texParameteri(bindingTarget, gl.TEXTURE_MAG_FILTER, gl.NEAREST);

    var targets = [gl.TEXTURE_2D];
    if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
        targets = [gl.TEXTURE_CUBE_MAP_POSITIVE_X,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_X,
                   gl.TEXTURE_CUBE_MAP_POSITIVE_Y,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_Y,
                   gl.TEXTURE_CUBE_MAP_POSITIVE_Z,
                   gl.TEXTURE_CUBE_MAP_NEGATIVE_Z];
    }
    // Upload the image into the texture
    for (var tt = 0; tt < targets.length; ++tt) {
        if (useTexSubImage2D) {
            // Initialize the texture to black first
            gl.texImage2D(targets[tt], 0, gl[internalFormat], bitmap.width,
                bitmap.height, 0, gl[pixelFormat], gl[pixelType], null);
            gl.texSubImage2D(targets[tt], 0, 0, 0, gl[pixelFormat],
                gl[pixelType], bitmap);
        } else {
            gl.texImage2D(targets[tt], 0, gl[internalFormat], gl[pixelFormat],
                gl[pixelType], bitmap);
        }
    }

    var width = gl.canvas.width;
    var height = gl.canvas.height;

    var loc;
    if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
        loc = gl.getUniformLocation(program, "face");
    }

    for (var tt = 0; tt < targets.length; ++tt) {
        if (bindingTarget == gl.TEXTURE_CUBE_MAP) {
            gl.uniform1i(loc, targets[tt]);
        }
        // Draw the triangles
        gl.clearColor(0, 0, 0, 1);
        gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        var buf = new Uint8Array(width * height * 4);
        gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, buf);
        pixelsBuffer.unshift({quality: testOptions.resizeQuality, flip: flipY,
            premul: premultiplyAlpha, buffer: buf});
    }
}

function runTestOnBindingTarget(bindingTarget, program, bitmaps, retVal,
                                testOptions) {
    var cases = [
        { sub: false, bitmap: bitmaps.defaultOption, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.defaultOption, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYPremul, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYPremul, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYDefault, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYDefault, flipY: false,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.noFlipYUnpremul, flipY: false,
            premultiply: false, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.noFlipYUnpremul, flipY: false,
            premultiply: false, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYPremul, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYPremul, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYDefault, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYDefault, flipY: true,
            premultiply: true, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.flipYUnpremul, flipY: true,
            premultiply: false, colorSpace: 'empty' },
        { sub: true, bitmap: bitmaps.flipYUnpremul, flipY: true,
            premultiply: false, colorSpace: 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceDef, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'notprovided' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceDef, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'notprovided' : 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceNone, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'none' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceNone, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'none' : 'empty' },
        { sub: false, bitmap: bitmaps.colorSpaceDefault, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'default' : 'empty' },
        { sub: true, bitmap: bitmaps.colorSpaceDefault, flipY: false,
            premultiply: true,
            colorSpace: retVal.colorSpaceEffect ? 'default' : 'empty' },
    ];

    for (var i = 0; i < cases.length; i++) {
        runOneIteration(cases[i].sub, bindingTarget, program, cases[i].bitmap,
            cases[i].flipY, cases[i].premultiply, retVal, cases[i].colorSpace,
            testOptions);
    }
}

// createImageBitmap resize code has two separate code paths for premul and
// unpremul image sources when the resize quality is set to high.
function runTest(bitmaps, alphaVal, colorSpaceEffective, testOptions)
{
    var retVal = {testPassed: true, alpha: alphaVal,
        colorSpaceEffect: colorSpaceEffective};
    var program = tiu.setupTexturedQuad(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_2D, program, bitmaps, retVal,
                           testOptions);
    program = tiu.setupTexturedQuadWithCubeMap(gl, internalFormat);
    runTestOnBindingTarget(gl.TEXTURE_CUBE_MAP, program, bitmaps, retVal,
                           testOptions);
    return retVal.testPassed;
}

function prepareResizedImageBitmapsAndRuntTest(testOptions) {
    var bitmaps = [];
    var imageSource= testOptions.imageSource;
    var options = {resizeWidth: testOptions.resizeWidth,
                   resizeHeight: testOptions.resizeHeight,
                   resizeQuality: testOptions.resizeQuality};
    var p1 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.defaultOption = imageBitmap });

    options.imageOrientation = "from-image";
    options.premultiplyAlpha = "premultiply";
    var p2 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYPremul = imageBitmap });

    options.premultiplyAlpha = "default";
    var p3 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYDefault = imageBitmap });

    options.premultiplyAlpha = "none";
    var p4 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.noFlipYUnpremul = imageBitmap });

    options.imageOrientation = "flipY";
    options.premultiplyAlpha = "premultiply";
    var p5 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYPremul = imageBitmap });

    options.premultiplyAlpha = "default";
    var p6 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYDefault = imageBitmap });

    options.premultiplyAlpha = "none";
    var p7 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.flipYUnpremul = imageBitmap });

    options = {resizeWidth: testOptions.resizeWidth,
               resizeHeight: testOptions.resizeHeight,
               resizeQuality: testOptions.resizeQuality};
    var p8 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceDef = imageBitmap });

    options.colorSpaceConversion = "none";
    var p9 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceNone = imageBitmap });

    options.colorSpaceConversion = "default";
    var p10 = createImageBitmap(imageSource, options).then(
        function(imageBitmap) { bitmaps.colorSpaceDefault = imageBitmap });

    return Promise.all([p1, p2, p3, p4, p5, p6, p7, p8, p9, p10]).then(
        function() {
            var alphaVal = 0.5;
            runTest(bitmaps, alphaVal, false, testOptions);
        });
}

function prepareResizedImageBitmapsAndRuntTests(testOptions) {
    testOptions.resizeQuality = resizeQualities[0];
    var p1 = prepareResizedImageBitmapsAndRuntTest(testOptions);
    testOptions.resizeQuality = resizeQualities[1];
    var p2 = prepareResizedImageBitmapsAndRuntTest(testOptions);
    testOptions.resizeQuality = resizeQualities[2];
    var p3 = prepareResizedImageBitmapsAndRuntTest(testOptions);
    testOptions.resizeQuality = resizeQualities[3];
    var p4 = prepareResizedImageBitmapsAndRuntTest(testOptions);

    return Promise.all([p1, p2, p3, p4]).then(function() {
            DrawResultsOnCanvas(testOptions);
    });
}

function prepareWebGLContext(testOptions) {
    var glcanvas = document.createElement('canvas');
    glcanvas.width = testOptions.resizeWidth;
    glcanvas.height = testOptions.resizeHeight;
    glcanvas.style.display="none";
    document.body.appendChild(glcanvas);
    gl = glcanvas.getContext("webgl");
    gl.clearColor(0,0,0,1);
    gl.clearDepth(1);
}

function PrintTileInfoForDebug(x, y, quality, premul, flip) {
    var tileLog = "x: " + x + ", y: " + y + ", quality: " + quality +
        ", premul: " + premul + ", flip: " + flip;
    console.log(tileLog);
}

function DrawResultsOnCanvas(testOptions) {
    var resultsCanvas = testOptions.resultsCanvas;
    var numTiles = Math.ceil(Math.sqrt(pixelsBuffer.length));
    var width = numTiles * testOptions.resizeWidth;
    var hieght = numTiles * testOptions.resizeWidth;
    var resultsCtx = resultsCanvas.getContext("2d");

    // Sweep for resize qualities one by one.
    var tileCounter = 0;
    for (var i = 0; i < resizeQualities.length; i++) {
        // Loop in reverse order and use splice to remove the buffer after
        // drawing to the canvas
        for (var j = pixelsBuffer.length - 1; j >= 0; j--) {
            if (pixelsBuffer[j].quality == resizeQualities[i]) {
                var buffer = pixelsBuffer[j].buffer;
                // Find the proper location for buffer
                var x = (tileCounter * testOptions.resizeWidth) % width;
                var y = Math.floor(tileCounter / numTiles) *
                    testOptions.resizeHeight;
                if (testOptions.printDebugInfoToConsole)
                    PrintTileInfoForDebug(x, y, pixelsBuffer[j].quality,
                        pixelsBuffer[j].premultiply, pixelsBuffer[j].flip);
                tileCounter++;
                var imageData = new ImageData(Uint8ClampedArray.from(buffer),
                    testOptions.resizeWidth, testOptions.resizeHeight);
                resultsCtx.putImageData(imageData, x, y);
                if (i != resizeQualities.length - 1)
                    pixelsBuffer.splice(j, 1);
            }
        }
    }
    if (window.testRunner)
        testRunner.notifyDone();
}
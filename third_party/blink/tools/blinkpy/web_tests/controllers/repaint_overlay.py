# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def result_contains_repaint_rects(text):
    return (isinstance(text, str) and
            re.search(r'"invalidations": \[$', text, re.MULTILINE) is not None)


def extract_layer_tree(input_str):
    if not isinstance(input_str, str):
        return '{}'

    if input_str[0:2] == '{\n':
        start = 0
    else:
        start = input_str.find('\n{\n')
        if start == -1:
            return '{}'

    end = input_str.find('\n}\n', start)
    if end == -1:
        return '{}'

    # FIXME: There may be multiple layer trees in the result.
    return input_str[start:end + 3]


def generate_repaint_overlay_html(test_name, actual_text, expected_text):
    if (not result_contains_repaint_rects(actual_text)
            and not result_contains_repaint_rects(expected_text)):
        return ''

    expected_layer_tree = extract_layer_tree(expected_text)
    actual_layer_tree = extract_layer_tree(actual_text)

    return """<!DOCTYPE HTML>
<html>
<head>
<title>%(title)s</title>
<style>
    body {
        margin: 0;
        padding: 0;
    }
    #overlay {
        width: 2000px;
        height: 2000px;
        border: 1px solid black;
    }
    #test_frame {
        position: absolute;
        width: 800px;
        height: 600px;
        border: 0;
        display: none;
    }
    canvas {
        position: absolute;
    }
    #actual_canvas {
        display: none;
    }
</style>
</head>
<body>
<label><input id="show-test" type="checkbox" onchange="toggle_test(this.checked)">Show test</label>
<label><input id="use-solid-colors" type="checkbox" onchange="toggle_solid_color(this.checked)">Use solid colors</label>
<br>
<span id="overlay_type">Expected Invalidations</span>
<div id="overlay">
    <iframe id="test_frame"></iframe>
    <canvas id="expected_canvas" width="2000" height="2000"></canvas>
    <canvas id="actual_canvas" width="2000" height="2000"></canvas>
</div>
<script>
var overlay_opacity = 0.25;

function toggle_test(show_test) {
    test_frame.style.display = show_test ? 'block' : 'none';
    if (!test_frame.src)
        test_frame.src = decodeURIComponent(location.search).substr(1);
}

function toggle_solid_color(use_solid_color) {
    overlay_opacity = use_solid_color ? 1 : 0.25;
    draw_repaint_rects();
}

var expected = %(expected)s;
var actual = %(actual)s;

function draw_rects(context, rects) {
    for (var i = 0; i < rects.length; ++i) {
        var rect = rects[i];
        context.fillRect(rect[0], rect[1], rect[2], rect[3]);
    }
}

function draw_layer_rects(context, transforms, layer) {
    context.save();
    var transform_path = [];
    for (var id = layer.transform; id; id = transforms[id].parent)
      transform_path.push(transforms[id]);

    for (var i = transform_path.length - 1; i >= 0; i--) {
        var m = transform_path[i].transform;
        if (!m)
          continue;
        var origin = transform_path[i].origin;
        if (origin)
            context.translate(origin[0], origin[1]);
        context.transform(m[0][0], m[0][1], m[1][0], m[1][1], m[3][0], m[3][1]);
        if (origin)
            context.translate(-origin[0], -origin[1]);
    }
    if (layer.position)
        context.translate(layer.position[0], layer.position[1]);
    if (layer.invalidations)
        draw_rects(context, layer.invalidations);
    context.restore();
}

function draw_result_rects(context, result) {
    var transforms = {};
    if (result.transforms) {
        for (var i = 0; i < result.transforms.length; ++i) {
            var transform = result.transforms[i];
            transforms[transform.id] = transform;
        }
    }
    if (result.layers) {
        for (var i = 0; i < result.layers.length; ++i)
            draw_layer_rects(context, transforms, result.layers[i]);
    }
}

function draw_repaint_rects() {
    var expected_ctx = expected_canvas.getContext("2d");
    expected_ctx.clearRect(0, 0, 2000, 2000);
    expected_ctx.fillStyle = 'rgba(255, 0, 0, ' + overlay_opacity + ')';
    draw_result_rects(expected_ctx, expected);

    var actual_ctx = actual_canvas.getContext("2d");
    actual_ctx.clearRect(0, 0, 2000, 2000);
    actual_ctx.fillStyle = 'rgba(0, 255, 0, ' + overlay_opacity + ')';
    draw_result_rects(actual_ctx, actual);
}

draw_repaint_rects();

var expected_showing = true;
function flip() {
    if (expected_showing) {
        overlay_type.textContent = 'Actual Invalidations';
        expected_canvas.style.display = 'none';
        actual_canvas.style.display = 'block';
    } else {
        overlay_type.textContent = 'Expected Invalidations';
        actual_canvas.style.display = 'none';
        expected_canvas.style.display = 'block';
    }
    expected_showing = !expected_showing
}
setInterval(flip, 1500);
</script>
</body>
</html>
""" % {
        'title': test_name,
        'expected': expected_layer_tree,
        'actual': actual_layer_tree,
    }

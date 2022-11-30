function _valToString(val)
{
    if (val === undefined || val === null)
        return '[' + typeof(val) + ']';
    return val.toString() + '[' + typeof(val) + ']';
}

var _hex2dec_table = {
    0:0, 1:1, 2:2, 3:3, 4:4, 5:5, 6:6, 7:7, 8:8, 9:9,
    a:10, b:11, c:12, d:13, e:14, f:15,
    A:10, B:11, C:12, D:13, E:14, F:15
};
function _hex2dec(hex)
{
    return _hex2dec_table[hex.charAt(0)]*16 + _hex2dec_table[hex.charAt(1)];
}

var _failed = false;
var _asserted = false;
function _warn(text)
{
    document.getElementById('d').appendChild(document.createElement('li')).appendChild(document.createTextNode(text));
}
function _fail(text)
{
    _warn(text);
    _failed = true;
}

function _assert(cond, text)
{
    _asserted = true;
    if (! cond)
        _fail('Failed assertion: ' + text);
}

function _assertSame(a, b, text_a, text_b)
{
    _asserted = true;
    if (a !== b)
        _fail('Failed assertion ' + text_a + ' === ' + text_b +
                ' (got ' + _valToString(a) + ', expected ' + _valToString(b) + ')');
}

function _assertDifferent(a, b, text_a, text_b)
{
    _asserted = true;
    if (a === b)
        _fail('Failed assertion ' + text_a + ' !== ' + text_b +
                ' (got ' + _valToString(a) + ', expected not ' + _valToString(b) + ')');
}

function _assertEqual(a, b, text_a, text_b)
{
    _asserted = true;
    if (a != b)
        _fail('Failed assertion ' + text_a + ' == ' + text_b +
                ' (got ' + _valToString(a) + ', expected ' + _valToString(b) + ')');
}

function _assertMatch(a, b, text_a, text_b)
{
    _asserted = true;
    if (! a.match(b))
        _fail('Failed assertion ' + text_a + ' matches ' + text_b +
                ' (got ' + _valToString(a) + ')');
}


var _manual_check = false;

function _requireManualCheck()
{
    _manual_check = true;
}

function _crash()
{
    _fail('Aborted due to predicted crash');
}

var _getImageDataCalibrated = false;
var _getImageDataIsPremul, _getImageDataIsBGRA;

function _getPixel(canvas, x,y)
{
    var ctx = canvas.getContext('2d');
    if (ctx && typeof(ctx.getImageData) != 'undefined')
    {
        try {
            var imgdata = ctx.getImageData(x, y, 1, 1);
        } catch (e) {
            // probably a security exception caused by having drawn
            // data: URLs onto the canvas
            imgdata = null;
        }
        if (imgdata)
        {
            // Work around getImageData bugs, since we want the other tests to
            // carry on working as well as possible
            if (! _getImageDataCalibrated)
            {
                var c2 = document.createElement('canvas');
                c2.width = c2.height = 1;
                var ctx2 = c2.getContext('2d');
                ctx2.fillStyle = 'rgba(0, 255, 255, 0.5)';
                ctx2.fillRect(0, 0, 1, 1);
                var data2 = ctx2.getImageData(0, 0, 1, 1).data;

                // Firefox returns premultiplied alpha

                if (data2[1] > 100 && data2[1] < 150)
                    _getImageDataIsPremul = true;
                else
                    _getImageDataIsPremul = false;

                // Opera Mini 4 Beta returns BGRA instead of RGBA

                if (data2[0] > 250 && data2[2] < 5)
                    _getImageDataIsBGRA = true;
                else
                    _getImageDataIsBGRA = false;

                _getImageDataCalibrated = true;
            }

            // Undo the BGRA flipping
            var rgba = (_getImageDataIsBGRA
                ? [ imgdata.data[2], imgdata.data[1], imgdata.data[0], imgdata.data[3] ]
                : [ imgdata.data[0], imgdata.data[1], imgdata.data[2], imgdata.data[3] ]);

            if (! _getImageDataIsPremul)
                return rgba;

            // Undo the premultiplying
            if (rgba[3] == 0)
                return [ 0, 0, 0, 0 ];
            else
            {
                var a = rgba[3] / 255;
                return [
                    Math.round(rgba[0]/a),
                    Math.round(rgba[1]/a),
                    Math.round(rgba[2]/a),
                    rgba[3]
                ];
            }
        }
    }

    try { ctx = canvas.getContext('opera-2dgame'); } catch (e) { /* Firefox throws */ }
    if (ctx && typeof(ctx.getPixel) != 'undefined')
    {
        try {
            var c = ctx.getPixel(x, y);
        } catch (e) {
            // probably a security exception caused by having drawn
            // data: URLs onto the canvas
            c = null;
        }
        if (c)
        {
            var matches = /^rgba\((\d+), (\d+), (\d+), ([\d\.]+)\)$/.exec(c);
            if (matches)
                return [ matches[1], matches[2], matches[3], Math.round(matches[4]*255) ];
            matches = /^#(..)(..)(..)$/.exec(c);
            if (matches)
                return [ _hex2dec(matches[1]), _hex2dec(matches[2]), _hex2dec(matches[3]), 255 ];
        }
    }
    //_warn("(Can't test pixel value)");
    _manual_check = true;
    return undefined;
}

function _assertPixel(canvas, x,y, r,g,b,a)
{
    _asserted = true;
    var c = _getPixel(canvas, x,y);
    if (c && ! (c[0] == r && c[1] == g && c[2] == b && c[3] == a))
        _fail('Failed assertion: got pixel [' + c + '] at ('+x+','+y+'), expected ['+r+','+g+','+b+','+a+']');
}

function _assertPixelApprox(canvas, x,y, r,g,b,a, tolerance)
{
    _asserted = true;
    var c = _getPixel(canvas, x,y);
    if (c)
    {
        var diff = Math.max(Math.abs(c[0]-r), Math.abs(c[1]-g), Math.abs(c[2]-b), Math.abs(c[3]-a));
        if (diff > tolerance)
            _fail('Failed assertion: got pixel [' + c + '] at ('+x+','+y+'), expected ['+r+','+g+','+b+','+a+'] +/- '+tolerance);
    }
}

function _addTest(test)
{
    var deferred = false;
    window.deferTest = function () { deferred = true; };
    function endTest()
    {
        if (_failed) // test failed
        {
            document.documentElement.className += ' fail';
            window._testStatus = ['fail', document.getElementById('d').innerHTML];
        }
        else if (_manual_check || !_asserted)
        { // test case explicitly asked for a manual check, or no automatic assertions were performed
            document.getElementById('d').innerHTML += '<li>Cannot automatically verify result';
            document.documentElement.className += ' needs_check';
            window._testStatus = ['check', document.getElementById('d').innerHTML];
        }
        else // test succeeded
        {
            document.getElementById('d').innerHTML += '<li>Passed';
            document.documentElement.className += ' pass';
            window._testStatus = ['pass', document.getElementById('d').innerHTML];
        }
        if (window.testRunner)
            testRunner.notifyDone();
    }
    window.endTest = endTest;
    window.wrapFunction = function (f)
    {
        return function()
        {
            try
            {
                f.apply(null, arguments);
            }
            catch (e)
            {
                _fail('Aborted with exception: ' + e.message);
            }
            endTest();
        }
    }

    window.onload = function ()
    {
        if (window.testRunner) {
            testRunner.dumpAsText();
            testRunner.waitUntilDone();
        }
        try
        {
            var canvas = document.getElementById('c');
            var ctx = canvas.getContext('2d');
            test(canvas, ctx);
        }
        catch (e)
        {
            _fail('Aborted with exception: ' + e.message);
            deferred = false; // cancel any deference
        }

        if (! deferred)
            endTest();
    };
}

'use strict';
// Tests that computed style is not affected by the zoom value.

function testProperty(data)
{
    var prop = data[0];
    if (data.length == 1)
        data.push('20px');

    for (var i = 1; i < data.length; i++) {
        testPropertyValue(prop, data[i]);
    }
}

function testPropertyValue(prop, value)
{
    test(() => {
        var el = document.createElement('div');
        el.style.cssText = 'position: absolute; width: 100px; height: 100px;' +
                           'overflow: hidden; border: 20px solid red;' +
                           'outline: 20px solid blue;-webkit-column-rule: 20px solid red';
        el.style.setProperty(prop, value, '');

        document.body.style.zoom = '';
        document.body.appendChild(el);

        var value1 = getComputedStyle(el, null).getPropertyValue(prop);
        document.body.style.zoom = 2;
        var value2 = getComputedStyle(el, null).getPropertyValue(prop);
        document.body.style.zoom = .5;
        var value3 = getComputedStyle(el, null).getPropertyValue(prop);

        document.body.removeChild(el);
        document.body.style.zoom = '';

        assert_equals(typeof value1, 'string');
        assert_equals(value1, value2);
        assert_equals(value2, value3);
    }, prop + ': ' + value);
}

var testData = [
    ['-webkit-border-horizontal-spacing'],
    ['-webkit-border-vertical-spacing'],
    ['-webkit-box-reflect', 'below 20px -webkit-gradient(linear, left top, left bottom, from(transparent), to(white))'],
    ['-webkit-box-shadow', '20px 20px 20px 20px red'],
    ['-webkit-column-rule-width', '20px'],
    ['-webkit-mask-box-image-outset'],
    ['-webkit-mask-box-image-width'],
    ['-webkit-mask-position-x', '20px', '-20px'],
    ['-webkit-mask-position-y', '20px', '-20px'],
    ['-webkit-perspective-origin', '20px 20px'],
    ['-webkit-text-stroke-width'],
    ['-webkit-transform', 'translate(20px, 20px)', 'translate3d(20px, 20px, 20px)', 'matrix3d(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)'],
    ['-webkit-transform-origin', '20px 20px', '-20px -20px'],
    ['background-position-x', '20px', '-20px'],
    ['background-position-y', '20px', '-20px'],
    ['border-bottom-left-radius'],
    ['border-bottom-right-radius'],
    ['border-bottom-width'],
    ['border-image-outset'],
    ['border-image-width'],
    ['border-left-width'],
    ['border-right-width'],
    ['border-spacing'],
    ['border-top-left-radius'],
    ['border-top-right-radius'],
    ['border-top-width'],
    ['bottom', '20px', '-20px'],
    ['clip', 'rect(20px 80px 80px 20px)'],
    ['flex-basis', '20px'],
    ['font-size', '20px', 'large'],
    ['height'],
    ['left', '20px', '-20px'],
    ['letter-spacing', '20px', '-20px'],
    ['line-height'],
    ['margin-bottom', '20px', '-20px'],
    ['margin-left', '20px', '-20px'],
    ['margin-right', '20px', '-20px'],
    ['margin-top', '20px', '-20px'],
    ['outline-width'],
    ['padding-bottom'],
    ['padding-left'],
    ['padding-right'],
    ['padding-top'],
    ['right', '20px', '-20px'],
    ['text-shadow', '20px 20px 20px red'],
    ['top', '20px', '-20px'],
    ['vertical-align', '20px', '-20px'],
    ['width'],
    ['word-spacing', '20px', '-20px'],
];

testData.forEach(testProperty);

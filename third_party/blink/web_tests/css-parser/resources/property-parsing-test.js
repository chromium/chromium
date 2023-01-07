// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// testharness.js and testharnessreport.js need to be included first

// Properties should be given in camelCase format

function convert_to_dashes(property) {
    return property.replace(/[A-Z]/g, function(letter) {
        return "-" + letter.toLowerCase();
    }).replace('webkit', '-webkit');
}

function assert_valid_value(property, value, serializedValue, quirksModeOnly) {
    if (arguments.length < 4)
        quirksModeOnly = false;

    if (arguments.length < 3)
        serializedValue = value;

    stringifiedValue = JSON.stringify(value);
    dashedProperty = convert_to_dashes(property);

    test(function(){
        assert_equals(!quirksModeOnly, CSS.supports(dashedProperty, value));
    }, "CSS.supports('" + dashedProperty + "', " + stringifiedValue + ") should return " + !quirksModeOnly);

    test(function(){
        var div = document.createElement('div');
        div.style[property] = value;
        assert_not_equals(div.style[property], "");
    }, "e.style['" + property + "'] = " + stringifiedValue + " should set the value");

    test(function(){
        var div = document.createElement('div');
        div.style[property] = value;
        var readValue = div.style[property];
        assert_equals(readValue, serializedValue);
        div.style[property] = readValue;
        assert_equals(div.style[property], readValue);
    }, "Serialization should round-trip after setting e.style['" + property + "'] = " + stringifiedValue);
}

function assert_invalid_value(property, value) {
    stringifiedValue = JSON.stringify(value);
    dashedProperty = convert_to_dashes(property);

    test(function(){
        assert_false(CSS.supports(dashedProperty, value));
    }, "CSS.supports('" + dashedProperty  + "', " + stringifiedValue + ") should return false");

    test(function(){
        var div = document.createElement('div');
        div.style[property] = value;
        assert_equals(div.style[property], "");
    }, "e.style['" + property + "'] = " + stringifiedValue + " should not set the value");
}

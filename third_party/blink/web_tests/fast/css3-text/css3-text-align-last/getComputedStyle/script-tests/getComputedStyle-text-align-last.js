function testElementStyle(propertyJS, propertyCSS, value)
{
    shouldBe("e.style." + propertyJS, "'" + value + "'");
}

function testComputedStyle(propertyJS, propertyCSS, value)
{
    computedStyle = window.getComputedStyle(e, null);
    shouldBe("computedStyle." + propertyJS, "'" + value + "'");
}

function valueSettingTest(value)
{
    debug("Value '" + value + "':");
    e.style.textAlignLast = value;
    testElementStyle("textAlignLast", "text-align-last", value);
    testComputedStyle("textAlignLast", "text-align-last", value);
    debug('');
}

function invalidValueSettingTest(value, defaultValue)
{
    debug("Invalid value test - '" + value + "':");
    e.style.textAlignLast = value;
    testElementStyle("textAlignLast", "text-align-last", defaultValue);
    testComputedStyle("textAlignLast", "text-align-last", defaultValue);
    debug('');
}

description("This test checks that text-align-last parses properly the properties from CSS 3 Text.");

e = document.getElementById('test');

debug("Test the initial value:");
testComputedStyle("textAlignLast", "text-align-last", 'auto');
debug('');

valueSettingTest('start');
valueSettingTest('end');
valueSettingTest('left');
valueSettingTest('right');
valueSettingTest('center');
valueSettingTest('justify');
valueSettingTest('match-parent');
valueSettingTest('auto');

defaultValue = 'auto'
e.style.textAlignLast = defaultValue;
invalidValueSettingTest('-webkit-left', defaultValue);
invalidValueSettingTest('-webkit-right', defaultValue);
invalidValueSettingTest('-webkit-center', defaultValue);
invalidValueSettingTest('-webkit-match-parent', defaultValue);
invalidValueSettingTest('-webkit-auto', defaultValue);

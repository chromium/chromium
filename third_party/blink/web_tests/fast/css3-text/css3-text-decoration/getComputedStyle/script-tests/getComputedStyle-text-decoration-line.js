function test(value, inlineValue, computedValue)
{
    if (value !== null)
        e.style.textDecorationLine = value;
    shouldBeEqualToString("e.style.textDecorationLine", inlineValue);
    computedStyle = window.getComputedStyle(e, null);
    shouldBeEqualToString("computedStyle.textDecorationLine", computedValue);
    debug("");
}

description("Test to make sure text-decoration-line is computed properly.")

var testContainer = document.createElement("div");
testContainer.contentEditable = true;
document.body.appendChild(testContainer);

testContainer.innerHTML = '<div id="test">hello world</div>';
e = document.getElementById('test');
debug("Initial value:");
test(null, "", "none");

debug("Initial value (explicit):");
test("initial", "initial", "none");

debug("Value 'none':");
test("none", "none", "none");

debug("Value 'underline':");
test("underline", "underline", "underline");

debug("Value 'overline':");
test("overline", "overline", "overline");

debug("Value 'line-through':");
test("line-through", "line-through", "line-through");

debug("Value 'blink' (valid, but ignored on computed style):");
test("blink", "blink", "blink");

debug("Value 'underline overline line-through blink':");
test("underline overline line-through blink", "underline overline line-through blink", "underline overline line-through blink");

debug("Value '':");
test("", "", "none");

testContainer.innerHTML = '<div id="test-parent" style="text-decoration-line: underline;">hello <span id="test-ancestor" style="text-decoration-line: inherit;">world</span></div>';
debug("Parent gets 'underline' value:");
e = document.getElementById('test-parent');
test(null, "underline", "underline");

debug("Ancestor should explicitly inherit value from parent when 'inherit' value is used:");
e = document.getElementById('test-ancestor');
test(null, "inherit", "underline");

debug("Ancestor should not implicitly inherit value from parent (i.e. when value is void):");
test("", "", "none");

document.body.removeChild(testContainer);

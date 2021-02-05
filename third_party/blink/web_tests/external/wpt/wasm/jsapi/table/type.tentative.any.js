// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/assertions.js

function assert_type(argument) {
    const mytable = new WebAssembly.Table(argument);

    assert_equals(mytable.type.minimum, argument.minimum);
    assert_equals(mytable.type.maximum, argument.maximum);
    assert_equals(mytable.type.element, argument.element);
}

test(() => {
    assert_type({ "minimum": 0, "element": "funcref"});
}, "Zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 5, "element": "funcref" });
}, "Non-zero initial, no maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 0, "element": "funcref" });
}, "Zero maximum");

test(() => {
    assert_type({ "minimum": 0, "maximum": 5, "element": "funcref" });
}, "None-zero maximum");

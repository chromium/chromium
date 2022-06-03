description("Test to ensure correct behaviour of Object.getOwnPropertyDescriptor");

function descriptorShouldBe(object, property, expected) {
    var test = 'Object.getOwnPropertyDescriptor('+object+", "+property+')';
    if ("writable" in expected) {
      //  shouldBe(test+'.writable', '' + expected.writable);
        shouldBe(test+'.value', '' + expected.value);
        shouldBeFalse(test+".hasOwnProperty('get')");
        shouldBeFalse(test+".hasOwnProperty('set')");
    } else {
        shouldBe(test+'.get', '' + expected.get);
        shouldBe(test+'.set', '' + expected.set);
        shouldBeFalse(test+".hasOwnProperty('value')");
        shouldBeFalse(test+".hasOwnProperty('writable')");
    }
    shouldBe(test+'.enumerable', '' + expected.enumerable);
    shouldBe(test+'.configurable', '' + expected.configurable);
}

shouldBeUndefined("Object.getOwnPropertyDescriptor({}, 'undefinedProperty')");
descriptorShouldBe("{definedProperty:'defined'}", "'definedProperty'", {writable: true, enumerable: true, configurable: true, value:'"defined"'});
descriptorShouldBe("Array.prototype", "'concat'", {writable: true, enumerable: false, configurable: true, value:"Array.prototype.concat"});
descriptorShouldBe("Date.prototype", "'toISOString'", {writable: true, enumerable: false, configurable: true, value: "Date.prototype.toISOString"});
descriptorShouldBe("String.prototype", "'concat'", {writable: true, enumerable: false, configurable: true, value:"String.prototype.concat"});
descriptorShouldBe("RegExp.prototype", "'exec'", {writable: true, enumerable: false, configurable: true, value:"RegExp.prototype.exec"});
descriptorShouldBe("document.__proto__.__proto__", "'createElement'", {writable: true, enumerable: true, configurable: true, value:"document.createElement"});
descriptorShouldBe("Number", "'NEGATIVE_INFINITY'", {writable: false, enumerable: false, configurable: false, value:"Number.NEGATIVE_INFINITY"});
descriptorShouldBe("RegExp", "'$_'", {writable: true, enumerable: false, configurable: true, value:"RegExp.$_"});
descriptorShouldBe("/a/g", "'global'", {writable: true, enumerable: false, configurable: false, value:true});
descriptorShouldBe("Node", "'DOCUMENT_POSITION_DISCONNECTED'", {writable: false, enumerable: true, configurable: false, value:"Node.DOCUMENT_POSITION_DISCONNECTED"});
descriptorShouldBe("Math", "'sin'", {writable: true, enumerable: false, configurable: true, value:"Math.sin"});
descriptorShouldBe("[1,2,3]", "0", {writable: true, enumerable: true, configurable: true, value:"1"});
descriptorShouldBe("[1,2,3]", "'length'", {writable: true, enumerable: false, configurable: false, value:"3"});
descriptorShouldBe("[1,2,3]", "'length'", {writable: true, enumerable: false, configurable: false, value:"3"});
descriptorShouldBe("/(a)*/g.exec('a')", "0", {writable: true, enumerable: true, configurable: true, value:"'a'"});
descriptorShouldBe("/(a)*/g.exec('a')", "'length'", {writable: true, enumerable: false, configurable: false, value:2});
descriptorShouldBe("function(){}", "'length'", {writable: false, enumerable: false, configurable: true, value:0});
descriptorShouldBe("Math.sin", "'length'", {writable: false, enumerable: false, configurable: true, value:1});
descriptorShouldBe("Math.sin", "'name'", {writable: false, enumerable: false, configurable: true, value:"'sin'"});
var global = this;
descriptorShouldBe("global", "'global'", {writable: true, enumerable: true, configurable: false, value:"global"});
descriptorShouldBe("global", "'undefined'", {writable: false, enumerable: false, configurable: false, value:"undefined"});
descriptorShouldBe("global", "'NaN'", {writable: false, enumerable: false, configurable: false, value:"NaN"});
descriptorShouldBe("global", "'Infinity'", {writable: false, enumerable: false, configurable: false, value:"Infinity"});
descriptorShouldBe("global", "'XMLHttpRequest'", {writable: true, enumerable: false, configurable: true, value:"XMLHttpRequest"});
descriptorShouldBe("global", "0", {writable: false, enumerable: true, configurable: true, value:"global[0]"});

var objectWithGetter = {};
function getterFunc(){};
objectWithGetter.__defineGetter__("getter", getterFunc);
descriptorShouldBe("objectWithGetter", "'getter'", {"get": "getterFunc", "set": undefined, enumerable: true, configurable: true});
var objectWithSetter = {};
function setterFunc(){};
objectWithSetter.__defineSetter__("setter", setterFunc);
descriptorShouldBe("objectWithSetter", "'setter'", {"set": "setterFunc", "get": undefined, enumerable: true, configurable: true});
var objectWithAccessor = {};
objectWithAccessor.__defineSetter__("accessor", setterFunc);
objectWithAccessor.__defineGetter__("accessor", getterFunc);
descriptorShouldBe("objectWithAccessor", "'accessor'", {"set": "setterFunc", "get": "getterFunc", enumerable: true, configurable: true});

shouldThrow("Object.getOwnPropertyDescriptor(null)");
shouldThrow("Object.getOwnPropertyDescriptor(undefined)");
shouldBeUndefined("Object.getOwnPropertyDescriptor(1)");
shouldBeUndefined("Object.getOwnPropertyDescriptor('')");
shouldBeUndefined("Object.getOwnPropertyDescriptor(true)");
shouldBeUndefined("Object.getOwnPropertyDescriptor(false)");

debug("Checking property ordering");
var normalOrder = ["'value'", "'writable'", "'enumerable'", "'configurable'"];
var accessorOrder = ["'get'", "'set'", "'enumerable'", "'configurable'"];
var i = 0;
for (var property in Object.getOwnPropertyDescriptor(Math, "sin"))
    shouldBe('property', normalOrder[i++]);
i = 0;
for (var property in Object.getOwnPropertyDescriptor(objectWithGetter, "getter"))
    shouldBe('property', accessorOrder[i++]);
i = 0;
for (var property in Object.getOwnPropertyDescriptor(objectWithSetter, "setter"))
    shouldBe('property', accessorOrder[i++]);
i = 0;
for (var property in Object.getOwnPropertyDescriptor(objectWithAccessor, "accessor"))
    shouldBe('property', accessorOrder[i++]);

description(
"instanceof test"
);

shouldBeTrue("(new Audio()) instanceof Audio");
shouldBeFalse("(new Array()) instanceof Audio");

shouldBeTrue("(new Image()) instanceof Image");
shouldBeFalse("(new Array()) instanceof Image");

// MessageChannel is not available yet.
//shouldBeTrue("(new MessageChannel()) instanceof MessageChannel");
//shouldBeFalse("(new Array()) instanceof MessageChannel");

shouldBeTrue("(new Option()) instanceof Option");
shouldBeFalse("(new Array()) instanceof Option");

shouldBeTrue("(new WebKitCSSMatrix()) instanceof WebKitCSSMatrix");
shouldBeFalse("(new Array()) instanceof WebKitCSSMatrix");

shouldBeTrue("(new Worker('instanceof-operator-dummy-worker.js')) instanceof Worker");
shouldBeFalse("(new Array()) instanceof Worker");

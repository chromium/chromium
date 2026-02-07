description("This tests the constructor property of DOM objects: http://bugs.webkit.org/show_bug.cgi?id=11315");

shouldBeFalse("window.document.constructor === window.Object");
shouldBeTrue("window.document.constructor === window.HTMLDocument");
shouldBeTrue("window.document.constructor.prototype === window.HTMLDocument.prototype");
shouldBeTrue("window.document.constructor.prototype.__proto__ === window.HTMLDocument.prototype.__proto__");
shouldBeTrue("window.document.constructor.prototype.__proto__ === window.Document.prototype");

shouldBeTrue("window.document.body.constructor === window.HTMLBodyElement");
shouldBeTrue("window.document.body.constructor.prototype === window.HTMLBodyElement.prototype");
shouldBeTrue("window.document.body.constructor.prototype.__proto__ === window.HTMLBodyElement.prototype.__proto__");
shouldBeTrue("window.document.body.constructor.prototype.__proto__ === window.HTMLElement.prototype");

var htmlCollection = document.getElementsByTagName('script');
shouldBeTrue("htmlCollection.constructor === window.HTMLCollection");
shouldBeTrue("htmlCollection.constructor.prototype === window.HTMLCollection.prototype");

var customEvent = new CustomEvent('test');
shouldBeTrue("customEvent.constructor === window.CustomEvent");
shouldBeTrue("customEvent.constructor.prototype.__proto__ === window.Event.prototype");

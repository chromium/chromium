description("This tests the init functions for all the event DOM classes that have them.");

function createEventHelper(prefix)
{
    var event = document.createEvent(prefix + "Event");
    return event;
}

function testInitEvent(prefix, argumentString)
{
    var event = createEventHelper(prefix);
    var initExpression = "event.init" + prefix + "Event(" + argumentString + ")";
    eval(initExpression);
    return event;
}

var event = document.createEvent("Event");

event.initEvent("type", false, false);

shouldBe("testInitEvent('', '\"a\", false, false').type", "'a'");
shouldBe("testInitEvent('', 'null, false, false').type", "'null'");
shouldBe("testInitEvent('', '\"a\", false, false').bubbles", "false");
shouldBe("testInitEvent('', '\"a\", true, false').bubbles", "true");
shouldBe("testInitEvent('', '\"a\", false, false').cancelable", "false");
shouldBe("testInitEvent('', '\"a\", false, true').cancelable", "true");

shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').type", "'a'");
shouldBe("testInitEvent('Keyboard', 'null, false, false, window, \"b\", 1001, false, false, false, false, false').type", "'null'");
shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').bubbles", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", true, false, window, \"b\", 1001, false, false, false, false, false').bubbles", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').cancelable", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').cancelable", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').view", "window");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, null, \"b\", 1001, false, false, false, false, false').view", "null");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').location", "1001");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').ctrlKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, true, false, false, false, false').ctrlKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').altKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, true, false, false, false').altKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').shiftKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, true, false, false').shiftKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').metaKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, true, false').metaKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, true').detail", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').keyCode", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').charCode", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').which", "0");

shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').type", "'a'");
shouldBe("testInitEvent('Message', 'null, false, false, \"b\", \"c\", \"d\", window, undefined').type", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').bubbles", "false");
shouldBe("testInitEvent('Message', '\"a\", true, false, \"b\", \"c\", \"d\", window, undefined').bubbles", "true");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').cancelable", "false");
shouldBe("testInitEvent('Message', '\"a\", false, true, \"b\", \"c\", \"d\", window, undefined').cancelable", "true");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').data", "'b'");
shouldBe("testInitEvent('Message', '\"a\", false, false, null, \"c\", \"d\", window, undefined').data", "null");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').origin", "'c'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", null, \"d\", window, undefined').origin", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').lastEventId", "'d'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", null, window, undefined').lastEventId", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, undefined').source", "window");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, undefined').source", "null");
var channel = new MessageChannel;
var channelArray = [channel.port1, channel.port2];
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, channelArray').ports[0]", "channel.port1");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, channelArray').ports[1]", "channel.port2");

shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').type", "'a'");
shouldBe("testInitEvent('Mouse', 'null, false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').type", "'null'");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').bubbles", "false");
shouldBe("testInitEvent('Mouse', '\"a\", true, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').bubbles", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').cancelable", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, true, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').cancelable", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').view", "window");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, null, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').view", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').detail", "1001");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').screenX", "1002");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').screenY", "1003");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').clientX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').clientY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').ctrlKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, true, false, false, false, 1006, null').ctrlKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').altKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, true, false, false, 1006, null').altKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').shiftKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, true, false, 1006, null').shiftKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').metaKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, true, 1006, null').metaKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').button", "1006");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').relatedTarget", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, document').relatedTarget", "document");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').offsetX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').offsetY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').x", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').y", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').fromElement", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, document').fromElement", "document");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').toElement", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').layerX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').layerY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').pageX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').pageY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').which", "1007");

shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').type", "'a'");
shouldBe("testInitEvent('Mutation', 'null, false, false, null, \"b\", \"c\", \"d\", 1001').type", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').bubbles", "false");
shouldBe("testInitEvent('Mutation', '\"a\", true, false, null, \"b\", \"c\", \"d\", 1001').bubbles", "true");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').cancelable", "false");
shouldBe("testInitEvent('Mutation', '\"a\", false, true, null, \"b\", \"c\", \"d\", 1001').cancelable", "true");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').relatedNode", "null");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, document, \"b\", \"c\", \"d\", 1001').relatedNode", "document");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').prevValue", "'b'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, null, \"c\", \"d\", 1001').prevValue", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').newValue", "'c'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", null, \"d\", 1001').newValue", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').attrName", "'d'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", null, 1001').attrName", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').attrChange", "1001");

shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').type", "'a'");
shouldBe("testInitEvent('Storage', 'null, false, false, \"b\", \"c\", \"d\", \"e\"').type", "'null'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').bubbles", "false");
shouldBe("testInitEvent('Storage', '\"a\", true, false, \"b\", \"c\", \"d\", \"e\"').bubbles", "true");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').cancelable", "false");
shouldBe("testInitEvent('Storage', '\"a\", false, true, \"b\", \"c\", \"d\", \"e\"').cancelable", "true");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').key", "'b'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, null, \"c\", \"d\", \"e\"').key", "null");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').oldValue", "'c'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", null, \"d\", \"e\"').oldValue", "null");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').newValue", "'d'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", null, \"e\"').newValue", "null");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\"').url", "'e'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", null').url", "'null'");

shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').type", "'a'");
shouldBe("testInitEvent('Text', 'null, false, false, window, \"b\"').type", "'null'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').bubbles", "false");
shouldBe("testInitEvent('Text', '\"a\", true, false, window, \"b\"').bubbles", "true");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').cancelable", "false");
shouldBe("testInitEvent('Text', '\"a\", false, true, window, \"b\"').cancelable", "true");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').view", "window");
shouldBe("testInitEvent('Text', '\"a\", false, false, null, \"b\"').view", "null");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').data", "'b'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, null').data", "'null'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').detail", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').which", "0");

shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').type", "'a'");
shouldBe("testInitEvent('UI', 'null, false, false, window, 1001').type", "'null'");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').bubbles", "false");
shouldBe("testInitEvent('UI', '\"a\", true, false, window, 1001').bubbles", "true");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').cancelable", "false");
shouldBe("testInitEvent('UI', '\"a\", false, true, window, 1001').cancelable", "true");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').view", "window");
shouldBe("testInitEvent('UI', '\"a\", false, false, null, 1001').view", "null");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').detail", "1001");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').which", "0");

// WheelEvent has no init function yet; roughly speaking, we are waiting for that part of DOM 3 to stabilize.

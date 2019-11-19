if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

description("This test checks the various use cases around sending multiple ports through MessagePort.postMessage");

var channel = new MessageChannel();
var channel2 = new MessageChannel();
var channel3 = new MessageChannel();
var channel4 = new MessageChannel();

shouldThrow("channel.port1.postMessage()");

channel.port1.postMessage("noport");
channel.port1.postMessage("zero ports", []);
channel.port1.postMessage("two ports", [channel2.port1, channel2.port2]);

// Now test various failure cases
shouldThrow('channel.port1.postMessage("same port", [channel.port1])', '"DataCloneError: Failed to execute \'postMessage\' on \'MessagePort\': Port at index 0 contains the source port."');
shouldThrow('channel.port1.postMessage("null port", [channel3.port1, null, channel3.port2])', '"TypeError: Failed to execute \'postMessage\' on \'MessagePort\': Value at index 1 is an untransferable \'null\' value."');
shouldThrow('channel.port1.postMessage("notAPort", [channel3.port1, {}, channel3.port2])', '"TypeError: Failed to execute \'postMessage\' on \'MessagePort\': Value at index 1 does not have a transferable type."');
shouldThrow('channel.port1.postMessage("duplicate port", [channel3.port1, channel3.port1])', '"DataCloneError: Failed to execute \'postMessage\' on \'MessagePort\': Message port at index 1 is a duplicate of an earlier port."');
// Should be OK to send channel3.port1 (should not have been disentangled by the previous failed calls).
channel.port1.postMessage("entangled ports", [channel3.port1, channel3.port2]);

shouldThrow('channel.port1.postMessage("notASequence", [{length: 3}])', '"TypeError: Failed to execute \'postMessage\' on \'MessagePort\': Value at index 0 does not have a transferable type."');
var arrayBuffer = new ArrayBuffer(2);
shouldThrow('channel.port1.postMessage("duplicate buffer", [arrayBuffer, arrayBuffer])', '"DataCloneError: Failed to execute \'postMessage\' on \'MessagePort\': ArrayBuffer at index 1 is a duplicate of an earlier ArrayBuffer."');

// Should not crash (we should figure out that the array contains undefined
// entries).
var largePortArray = [];
largePortArray[1234567890] = channel4.port1;
shouldThrow('channel.port1.postMessage("largeSequence", largePortArray)', '"RangeError: Failed to execute \'postMessage\' on \'MessagePort\': Array length exceeds supported limit."');

channel.port1.postMessage("done");

function testTransfers() {
    var channel0 = new MessageChannel();

    var c1 = new MessageChannel();
    channel0.port1.postMessage({id:"send-port", port:c1.port1}, [c1.port1]);
    var c2 = new MessageChannel();
    channel0.port1.postMessage({id:"send-port-twice", port0:c2.port1, port1:c2.port1}, [c2.port1]);
    var c3 = new MessageChannel();
    channel0.port1.postMessage({id:"send-two-ports", port0:c3.port1, port1:c3.port2}, [c3.port1, c3.port2]);
    var c4 = new MessageChannel();
    try {
        channel0.port1.postMessage({id:"host-object", hostObject:c3, port:c4.port1}, [c4.port1]);
        testFailed("Sending host object should throw");
    } catch(e) {
        if (e.code == DOMException.DATA_CLONE_ERR)
          testPassed("Sending host object has thrown " + e);
        else
          testFailed("Sending host object should throw a DataCloneError, got: " + e);
    }
    try {
        channel0.port1.postMessage({id:"host-object2", hostObject:navigator, port:c4.port1}, [c4.port1]);
        testFailed("Sending host object should throw");
    } catch(e) {
        if (e.code == DOMException.DATA_CLONE_ERR)
          testPassed("Sending host object has thrown " + e);
        else
          testFailed("Sending host object should throw a DataCloneError, got: " + e);
    }
    try {
        var f1 = function() {}
        channel0.port1.postMessage({id:"function-object", function:f1, port:c4.port1}, [c4.port1]);
        testFailed("Sending Function object should throw");
    } catch(e) {
        if (e.code == DOMException.DATA_CLONE_ERR)
          testPassed("Sending Function object has thrown " + e);
        else
          testFailed("Sending Function object should throw a DataCloneError, got: " + e);
    }
    c4.port1.postMessage("Should succeed");
    channel0.port1.postMessage({id:"done"});

    channel0.port2.onmessage = function(event) {
        if (event.data.id == "send-port") {
            if (event.ports && event.ports.length > 0 && event.ports[0] === event.data.port)
                testPassed("send-port: transferred one port");
            else
                testFailed("send-port: port transfer failed");
        } else if (event.data.id == "send-port-twice") {
            if (event.ports && event.ports.length == 1 &&
                  event.ports[0] === event.data.port0 && event.ports[0] === event.data.port1)
                testPassed("send-port-twice: transferred one port twice");
            else
                testFailed("send-port-twice: failed to transfer one port twice");
        } else if (event.data.id == "send-two-ports") {
            if (event.ports && event.ports.length == 2 &&
                  event.ports[0] === event.data.port0 && event.ports[1] === event.data.port1)
                testPassed("send-two-ports: transferred two ports");
            else
                testFailed("send-two-ports: failed to transfer two ports");
        } else if (event.data.id == "done") {
            debug('<br><span class="pass">TEST COMPLETE</span>');
            if (window.testRunner)
            testRunner.notifyDone();
        } else {
            testFailed("Unexpected message " + event.data);
        }
    }
}

channel.port2.onmessage = function(event) {
    if (event.data == "noport") {
        if (event.ports && !event.ports.length)
            testPassed("event.ports is non-null and zero length when no port sent");
        else
            testFailed("event.ports is null or non-zero length when no port sent");
    } else if (event.data == "zero ports") {
        if (event.ports && !event.ports.length)
            testPassed("event.ports is non-null and zero length when empty array sent");
        else
            testFailed("event.ports is null or non-zero length when empty array sent");
    } else if (event.data == "two ports") {
        if (!event.ports) {
            testFailed("event.ports should be non-null when ports sent");
            return;
        }
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports sent");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports sent");

    } else if (event.data == "entangled ports") {
        if (event.ports.length == 2)
            testPassed("event.ports contains two ports when two ports re-sent after error");
        else
            testFailed("event.ports contained " + event.ports.length + " when two ports re-sent after error");
    } else if (event.data == "done") {
        testTransfers();
    } else
        testFailed("Received unexpected message: " + event.data);
}

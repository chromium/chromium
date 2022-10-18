function logError(error)
{
    debug("error is: " + error.toString());
}

// Verifies that the given "bytes" holds the same value as "expectedHexString".
// "bytes" can be anything recognized by "bytesToHexString()".
function bytesShouldMatchHexString(testDescription, expectedHexString, bytes)
{
    expectedHexString = "[" + expectedHexString.toLowerCase() + "]";
    var actualHexString = "[" + bytesToHexString(bytes) + "]";

    if (actualHexString === expectedHexString) {
        debug("PASS: " + testDescription + " should be " + expectedHexString + " and was");
    } else {
        debug("FAIL: " + testDescription + " should be " + expectedHexString + " but was " + actualHexString);
    }
}

// Builds a hex string representation for an array-like input.
// "bytes" can be an Array of bytes, an ArrayBuffer, or any TypedArray.
// The output looks like this:
//    ab034c99
function bytesToHexString(bytes)
{
    if (!bytes)
        return null;
    // "bytes" could be a typed array with a detached buffer, in which case the
    // constructor below would throw.
    if (bytes.length === 0)
        return "";

    bytes = new Uint8Array(bytes);
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return hexBytes.join("");
}

function bytesToASCIIString(bytes)
{
    return String.fromCharCode.apply(null, new Uint8Array(bytes));
}

function hexStringToUint8Array(hexString)
{
    if (hexString.length % 2 != 0)
        throw "Invalid hexString";
    var arrayBuffer = new Uint8Array(hexString.length / 2);

    for (var i = 0; i < hexString.length; i += 2) {
        var byteValue = parseInt(hexString.substr(i, 2), 16);
        if (byteValue == NaN)
            throw "Invalid hexString";
        arrayBuffer[i/2] = byteValue;
    }

    return arrayBuffer;
}

function asciiToUint8Array(str)
{
    var chars = [];
    for (var i = 0; i < str.length; ++i)
        chars.push(str.charCodeAt(i));
    return new Uint8Array(chars);
}

var Base64URL = {
    stringify: function (a) {
        var base64string = btoa(String.fromCharCode.apply(0, a));
        return base64string.replace(/=/g, "").replace(/\+/g, "-").replace(/\//g, "_");
    },
    parse: function (s) {
        s = s.replace(/-/g, "+").replace(/_/g, "/").replace(/\s/g, '');
        return new Uint8Array(Array.prototype.map.call(atob(s), function (c) { return c.charCodeAt(0) }));
    }
};

function failAndFinishJSTest(error)
{
    testFailed('' + error);
    finishJSTest();
}

// Returns a Promise for the cloned key.
function cloneKey(key)
{
    // Sending an object through a MessagePort implicitly clones it.
    // Use a single MessageChannel so requests complete in FIFO order.
    var self = cloneKey;
    if (!self.channel) {
        self.channel = new MessageChannel();
        self.callbacks = [];
        self.channel.port1.addEventListener('message', function(e) {
            var callback = self.callbacks.shift();
            callback(e.data);
        }, false);
        self.channel.port1.start();
    }

    return new Promise(function(resolve, reject) {
        self.callbacks.push(resolve);
        self.channel.port2.postMessage(key);
    });
}

// Logging the serialized format ensures that if it changes it will break tests.
function logSerializedKey(o)
{
    if (internals) {
        // Removing the version tag from the output so serialization format changes don't need to update all the crypto tests.
        var serialized = internals.serializeObject(o);
        var serializedWithoutVersion = new Uint8Array(serialized, 17);
        debug("Serialized key bytes: " + bytesToHexString(serializedWithoutVersion));
    }
}

function shouldEvaluateAs(actual, expectedValue)
{
    if (typeof expectedValue == "string")
        return shouldBeEqualToString(actual, expectedValue);
    return shouldEvaluateTo(actual, expectedValue);
}

// expectFailure() is a helper to convert a Promise that is expect
// to fail into one that is epxected to succeed, for use in chaining
// promises.
//
// For instance:
//
//    // Catches and logs the failure from importKey().
//    return expectFailue("Import key...", crypto.subtle.importKey(...));
function expectFailure(message, promise)
{
    debug("\n" + message);

    return promise.then(function(result) {
        debug("FAILED: Expected failure but got: " + result);

        // Re-throw the error.
        throw new Error("Test FAILED: " + message);
    }, function(error) {
        // Expected to fail, so consider this a success.
        debug("SUCCESS (rejected): " + error.toString());
        return error;
    });
}

function expectSuccess(message, promise)
{
    debug("\n" + message);

    return promise.then(function(result) {
      // Expected to succeed.
      debug("SUCCESS");
      return result;
    }, function(error) {
      debug("FAILED: " + error.toString());

      // Re-throw the error.
      throw new Error("Test FAILED: " + message);
    });
}

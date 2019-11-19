// Try calling the browser-side bound functions with varying (invalid)
// inputs. There is no notion of "success" for this test, other than
// verifying the correct C++ bindings were reached with expected values.

function MyObject() {
  this.x = "3";
}

MyObject.prototype.toString = function() {
  throw "exception from calling toString()";
}

function expectEquals(expectation, actual) {
  if (!(expectation === actual)) {
    throw "FAIL: expected: " + expectation + ", actual: " + actual;
  }
}

function FindProxyForURL(url, host) {
  // Call dnsResolve with some wonky arguments.
  // Those expected to fail (because we have passed a non-string parameter)
  // will return |null|, whereas those that have called through to the C++
  // bindings will return '127.0.0.1'.
  expectEquals(null, dnsResolve());
  expectEquals(null, dnsResolve(null));
  expectEquals(null, dnsResolve(undefined));
  expectEquals('127.0.0.1', dnsResolve(""));
  expectEquals(null, dnsResolve({foo: 'bar'}));
  expectEquals(null, dnsResolve(fn));
  expectEquals(null, dnsResolve(['3']));
  expectEquals('127.0.0.1', dnsResolve("arg1", "arg2", "arg3", "arg4"));

  // Call alert with some wonky arguments.
  alert();
  alert(null);
  alert(undefined);
  alert({foo:'bar'});

  // This should throw an exception when we toString() the argument
  // to alert in the bindings.
  try {
    alert(new MyObject());
  } catch (e) {
    alert(e);
  }

  // Call myIpAddress() with wonky arguments
  myIpAddress(null);
  myIpAddress(null, null);

  // Call myIpAddressEx() correctly (no arguments).
  myIpAddressEx();

  // Call dnsResolveEx() (note that isResolvableEx() implicity calls it.)
  isResolvableEx("is_resolvable");
  dnsResolveEx("foobar");

  return "DIRECT";
}

function fn() {}


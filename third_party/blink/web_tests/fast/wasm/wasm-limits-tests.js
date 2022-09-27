// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var limit = Math.pow(2, 12);

function NoParameters() {
  function ExpectTypeError(f) {
    try {
      f();
    } catch (e) {
      assert_true(e instanceof TypeError)
      return;
    }
    assert_unreached();
  }
  ExpectTypeError(() => new WebAssembly.Module());
  ExpectTypeError(() => new WebAssembly.Module("a"));
  ExpectTypeError(() => new WebAssembly.Instance());
  ExpectTypeError(() => new WebAssembly.Instance("a"));
}

function NoParameters_Promise() {
  function ExpectTypeError(f) {
    return f().then(assert_unreached, e => assert_true(e instanceof TypeError));
  }
  return Promise.all([
    ExpectTypeError(() => WebAssembly.compile()),
    ExpectTypeError(() => WebAssembly.compile("a")),
    ExpectTypeError(() => WebAssembly.instantiate()),
    ExpectTypeError(() => WebAssembly.instantiate("a"))
  ]);
}

function TestBuffersAreCorrect() {
  var buffs = createTestBuffers(limit);
  assert_equals(buffs.small.byteLength, limit);
  assert_equals(buffs.large.byteLength, limit + 1);
}

function compileFailsWithError(buffer, error_type) {
  try {
    new WebAssembly.Module(buffer);
  } catch (e) {
    assert_true(e instanceof error_type);
  }
}

function TestSyncCompile() {
  var buffs = createTestBuffers(limit);
  assert_true(new WebAssembly.Module(buffs.small)
              instanceof WebAssembly.Module);
  compileFailsWithError(buffs.large, RangeError);
}

function TestPromiseCompile() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then(m => assert_true(m instanceof WebAssembly.Module));
}

function TestWorkerCompileAndInstantiate() {
  var worker = new Worker("wasm-limits-worker.js");
  return new Promise((resolve, reject) => {
    worker.postMessage(createTestBuffers(limit).large);
    worker.onmessage = function(event) {
      resolve(event.data);
    }
  })
    .then(ans => assert_true(ans),
          assert_unreached);
}

function TestPromiseCompileSyncInstantiate() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then (m => new WebAssembly.Instance(m))
    .then(assert_unreached,
          e => assert_true(e instanceof RangeError));
}

function TestPromiseCompileAsyncInstantiateFromBuffer() {
  return WebAssembly.instantiate(createTestBuffers(limit).large)
    .then(i => assert_true(i.instance instanceof WebAssembly.Instance),
          assert_unreached);
}

function TestPromiseCompileAsyncInstantiateFromModule() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then(m => {
      assert_true(m instanceof WebAssembly.Module);
      return WebAssembly.instantiate(m).
        then(i => assert_true(i instanceof WebAssembly.Instance),
             assert_unreached);
    },
    assert_unreached);
}


function TestCompileFromPromise() {
  return Promise.resolve(createTestBuffers(limit).large)
    .then(WebAssembly.compile)
    .then(m => assert_true(m instanceof WebAssembly.Module))
}

function TestInstantiateFromPromise() {
  return Promise.resolve(createTestBuffers(limit).large)
    .then(WebAssembly.instantiate)
    .then(pair => {
      assert_true(pair.module instanceof WebAssembly.Module);
      assert_true(pair.instance instanceof WebAssembly.Instance);
    });
}

function TestInstantiateFromPromiseChain() {
  return Promise.resolve(createTestBuffers(limit).large)
    .then(WebAssembly.compile)
    .then(WebAssembly.instantiate)
    .then(i => assert_true(i instanceof WebAssembly.Instance))
}

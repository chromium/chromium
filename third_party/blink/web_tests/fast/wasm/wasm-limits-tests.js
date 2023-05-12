// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const limit = Math.pow(2, 23);

const kWasmH0 = 0;
const kWasmH1 = 0x61;
const kWasmH2 = 0x73;
const kWasmH3 = 0x6d;

const kWasmV0 = 0x1;
const kWasmV1 = 0;
const kWasmV2 = 0;
const kWasmV3 = 0;

function wasmSignedLeb(val, max_len = 5) {
  if (val == null) throw new Error("Leb value many not be null/undefined");
  let res = [];
  for (let i = 0; i < max_len; ++i) {
    let v = val & 0x7f;
    // If {v} sign-extended from 7 to 32 bits is equal to val, we are done.
    if (((v << 25) >> 25) == val) {
      res.push(v);
      return res;
    }
    res.push(v | 0x80);
    val = val >> 7;
  }
  throw new Error(
      'Leb value <' + val + '> exceeds maximum length of ' + max_len);
}

function createTestBuffer(limit) {
  const buffer = new Uint8Array(limit);
  const header = [
    kWasmH0, kWasmH1, kWasmH2, kWasmH3, // magic word
    kWasmV0, kWasmV1, kWasmV2, kWasmV3, // version
    0                                   // custom section
  ];
  // We calculate the section length so that the total module size is `limit`.
  // For that we have to calculate the length of the leb encoding of the section
  // length.
  const sectionLength = limit - header.length -
      wasmSignedLeb(limit).length;
  const lengthBytes = wasmSignedLeb(sectionLength);
  buffer.set(header);
  buffer.set(lengthBytes, header.length);
  return buffer;
}

function NoParameters() {
  assert_throws_js(TypeError, () => new WebAssembly.Module());
  assert_throws_js(TypeError, () => new WebAssembly.Module("a"));
  assert_throws_js(TypeError, () => new WebAssembly.Instance());
  assert_throws_js(TypeError, () => new WebAssembly.Instance("a"));
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

function TestSyncCompile() {
  assert_true(new WebAssembly.Module(createTestBuffer(limit))
              instanceof WebAssembly.Module);
  assert_throws_js(
      RangeError, () => new WebAssembly.Module(createTestBuffer(limit + 1)));
}

function TestPromiseCompile() {
  return WebAssembly.compile(createTestBuffer(limit + 1))
    .then(m => assert_true(m instanceof WebAssembly.Module));
}

function WorkerCode() {
  onmessage = (event) => {
    const buffer = event.data;
    try {
      let module = new WebAssembly.Module(buffer);
      let instance = new WebAssembly.Instance(module);
      postMessage(
          module instanceof WebAssembly.Module &&
          instance instanceof WebAssembly.Instance);
    } catch (e) {
      postMessage(false);
    }
  }
}

function TestWorkerCompileAndInstantiate() {
  const workerBlob = new Blob(['(', WorkerCode.toString(), ')()']);
  const blobUrl = blobURL = URL.createObjectURL(
      workerBlob, {type: 'application/javascript; charset=utf-8'});
  const worker = new Worker(blobUrl);
  return new Promise((resolve, reject) => {
    worker.postMessage(createTestBuffer(limit + 1));
    worker.onmessage = function(event) {
      resolve(event.data);
    }
  })
    .then(ans => assert_true(ans),
          assert_unreached);
}

function TestPromiseCompileSyncInstantiate() {
  return WebAssembly.compile(createTestBuffer(limit + 1))
      .then(
          m => assert_throws_js(RangeError, () => new WebAssembly.Instance(m)));
}

function TestPromiseCompileAsyncInstantiateFromBuffer() {
  return WebAssembly.instantiate(createTestBuffer(limit + 1))
    .then(i => assert_true(i.instance instanceof WebAssembly.Instance),
          assert_unreached);
}

function TestPromiseCompileAsyncInstantiateFromModule() {
  return WebAssembly.compile(createTestBuffer(limit + 1)).then(m => {
    assert_true(m instanceof WebAssembly.Module);
    return WebAssembly.instantiate(m).then(
        i => assert_true(i instanceof WebAssembly.Instance), assert_unreached);
  }, assert_unreached);
}


function TestCompileFromPromise() {
  return Promise.resolve(createTestBuffer(limit + 1))
    .then(WebAssembly.compile)
    .then(m => assert_true(m instanceof WebAssembly.Module))
}

function TestInstantiateFromPromise() {
  return Promise.resolve(createTestBuffer(limit + 1))
    .then(WebAssembly.instantiate)
    .then(pair => {
      assert_true(pair.module instanceof WebAssembly.Module);
      assert_true(pair.instance instanceof WebAssembly.Instance);
    });
}

function TestInstantiateFromPromiseChain() {
  return Promise.resolve(createTestBuffer(limit + 1))
    .then(WebAssembly.compile)
    .then(WebAssembly.instantiate)
    .then(i => assert_true(i instanceof WebAssembly.Instance))
}

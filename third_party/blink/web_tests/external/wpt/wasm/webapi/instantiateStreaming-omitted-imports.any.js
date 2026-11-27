// META: global=window,worker
// META: script=/wasm/jsapi/wasm-module-builder.js

promise_test(async t => {
  const builder = new WasmModuleBuilder();
  builder.addImport("omittedImport", "f", kSig_v_v);
  const buffer = builder.toBuffer();
  const response = new Response(buffer, { "headers": { "Content-Type": "application/wasm" } });
  await promise_rejects_js(t, TypeError, WebAssembly.instantiateStreaming(response));
}, "Omitted import object rejects a module that has imports");

promise_test(async () => {
  const buffer = new WasmModuleBuilder().toBuffer();
  const response = new Response(buffer, { "headers": { "Content-Type": "application/wasm" } });
  const result = await WebAssembly.instantiateStreaming(response);
  assert_equals(Object.getPrototypeOf(result.module), WebAssembly.Module.prototype);
  assert_equals(Object.getPrototypeOf(result.instance), WebAssembly.Instance.prototype);
}, "Omitted import object instantiates a module with no imports");

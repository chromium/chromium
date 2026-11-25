// META: global=window,dedicatedworker,jsshell

promise_test(async (t) => {
  await promise_rejects_js(
    t,
    SyntaxError,
    import("./resources/resolve-export.js")
  );
}, "ResolveExport on invalid re-export from WebAssembly");

for (const [name, description] of [
  ["missing-js", "an export missing from a JavaScript module"],
  ["missing-wasm", "an export missing from a WebAssembly module"],
  ["ambiguous", "an ambiguous star export"],
]) {
  promise_test(async (t) => {
    await promise_rejects_js(
      t,
      SyntaxError,
      import(`./resources/resolve-import-${name}.wasm`)
    );
    assert_false(
      "resolveImportTargetEvaluated" in globalThis,
      "dependencies are not evaluated"
    );
  }, `WebAssembly import of ${description} fails to link`);
}

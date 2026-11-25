// META: global=window,dedicatedworker,jsshell
// META: script=/wasm/jsapi/wasm-module-builder.js

promise_test(async () => {
  const tag = new WebAssembly.Tag({ parameters: ["i32"] });
  const builder = new WasmModuleBuilder();
  const tagIndex = builder.addImportedTag("imp", "tag", kSig_v_i);
  builder.addExportOfKind("tag", kExternalTag, tagIndex);
  const { instance } = await WebAssembly.instantiate(builder.toBuffer(), { imp: { tag } });
  assert_equals(instance.exports.tag, tag);
}, "Re-exported imported tag is the same object");

promise_test(async () => {
  const producer = new WasmModuleBuilder();
  const producedIndex = producer.addTag(kSig_v_i);
  producer.addExportOfKind("tag", kExternalTag, producedIndex);
  const produced = await WebAssembly.instantiate(producer.toBuffer());

  const importer = new WasmModuleBuilder();
  const importedIndex = importer.addImportedTag("imp", "tag", kSig_v_i);
  importer.addExportOfKind("tag", kExternalTag, importedIndex);
  const { instance } = await WebAssembly.instantiate(importer.toBuffer(), {
    imp: { tag: produced.instance.exports.tag },
  });
  assert_equals(instance.exports.tag, produced.instance.exports.tag);
}, "Re-exported tag from another instance is the same object");

promise_test(async () => {
  const builder = new WasmModuleBuilder();
  const tagIndex = builder.addTag(kSig_v_i);
  builder.addExportOfKind("a", kExternalTag, tagIndex);
  builder.addExportOfKind("b", kExternalTag, tagIndex);
  const { instance } = await WebAssembly.instantiate(builder.toBuffer());
  assert_equals(instance.exports.a, instance.exports.b);
  assert_equals(Object.getPrototypeOf(instance.exports.a), WebAssembly.Tag.prototype);
}, "Two exports of one local tag are the same object");

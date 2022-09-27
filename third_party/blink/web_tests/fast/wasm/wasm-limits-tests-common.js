// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createTestBuffers(limit) {
  let builder = new WasmModuleBuilder();
  let body = [];
  var fct = builder.addFunction("f", kSig_v_v);

  var l = builder.toBuffer().byteLength;

  // in the bare bones buffer, the size of the function f
  // is 0, which is encoded in 1 byte. For the 2^12 case,
  // we need 2 bytes. Then, the function ends in kExprEnd,
  // so we need that accounted, too.
  var remaining = limit - l - 3;

  for (var i = 0; i < remaining; ++i) body.push(kExprNop);
  fct.addBody(body);

  var small_buffer = builder.toBuffer();
  // body is now 1 larger than before, because it has the kExpEnd at the end.
  // replace that with kExprNop, and generate a new buffer.
  body[body.length-1] = kExprNop;
  fct.addBody(body);
  var large_buffer = builder.toBuffer();
  return {small: small_buffer, large: large_buffer};
}

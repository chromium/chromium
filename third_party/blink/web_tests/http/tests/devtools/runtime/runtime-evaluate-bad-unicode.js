// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests TestRunner.RuntimeAgent.evaluate can handle invalid Unicode code points and non-characters.\n`);

  async function test(expression) {
    const executionContext = UIModule.Context.Context.instance().flavor(SDK.RuntimeModel.ExecutionContext);
    const compileResult = await executionContext.runtimeModel.compileScript(expression, '', true, executionContext.id);
    const runResult = await executionContext.runtimeModel.runScript(compileResult.scriptId, executionContext.id);
    TestRunner.addResult(`"${expression}" -> ${runResult.object.value}`);
  }

  // Invalid code points, i.e. code points that are not Unicode scalar
  // values. https://unicode.org/glossary/#unicode_scalar_value
  {
    // High surrogates: U+D800..U+DBFF
    await test("'\uD800' === '\\uD800'");
    await test("'\uD800'.codePointAt(0).toString(16)");
    await test("'\uDBFF' === '\\uDBFF'");
    await test("'\uDBFF'.codePointAt(0).toString(16)");

    // Low surrogates: U+DC00..U+DFFF
    await test("'\uDC00' === '\\uDC00'");
    await test("'\uDC00'.codePointAt(0).toString(16)");
    await test("'\uDFFF' === '\\uDFFF'");
    await test("'\uDFFF'.codePointAt(0).toString(16)");
  }

  // Unicode non-characters:
  // https://unicode.org/faq/private_use.html#nonchar1
  {
    await test("'\uFDD0' === '\\uFDD0'");
    await test("'\uFDD0'.codePointAt(0).toString(16)");
    await test("'\uFDEF' === '\\uFDEF'");
    await test("'\uFDEF'.codePointAt(0).toString(16)");

    await test("'\uFFFE' === '\\uFFFE'");
    await test("'\uFFFE'.codePointAt(0).toString(16)");
    await test("'\uFFFF' === '\\uFFFF'");
    await test("'\uFFFF'.codePointAt(0).toString(16)");

    await test("'\u{01FFFE}' === '\\u{01FFFE}'");
    await test("'\u{01FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{01FFFF}' === '\\u{01FFFF}'");
    await test("'\u{01FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{02FFFE}' === '\\u{02FFFE}'");
    await test("'\u{02FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{02FFFF}' === '\\u{02FFFF}'");
    await test("'\u{02FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{03FFFE}' === '\\u{03FFFE}'");
    await test("'\u{03FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{03FFFF}' === '\\u{03FFFF}'");
    await test("'\u{03FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{04FFFE}' === '\\u{04FFFE}'");
    await test("'\u{04FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{04FFFF}' === '\\u{04FFFF}'");
    await test("'\u{04FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{05FFFE}' === '\\u{05FFFE}'");
    await test("'\u{05FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{05FFFF}' === '\\u{05FFFF}'");
    await test("'\u{05FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{06FFFE}' === '\\u{06FFFE}'");
    await test("'\u{06FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{06FFFF}' === '\\u{06FFFF}'");
    await test("'\u{06FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{07FFFE}' === '\\u{07FFFE}'");
    await test("'\u{07FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{07FFFF}' === '\\u{07FFFF}'");
    await test("'\u{07FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{08FFFE}' === '\\u{08FFFE}'");
    await test("'\u{08FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{08FFFF}' === '\\u{08FFFF}'");
    await test("'\u{08FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{09FFFE}' === '\\u{09FFFE}'");
    await test("'\u{09FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{09FFFF}' === '\\u{09FFFF}'");
    await test("'\u{09FFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0AFFFE}' === '\\u{0AFFFE}'");
    await test("'\u{0AFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0AFFFF}' === '\\u{0AFFFF}'");
    await test("'\u{0AFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0BFFFE}' === '\\u{0BFFFE}'");
    await test("'\u{0BFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0BFFFF}' === '\\u{0BFFFF}'");
    await test("'\u{0BFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0BFFFE}' === '\\u{0BFFFE}'");
    await test("'\u{0BFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0BFFFF}' === '\\u{0BFFFF}'");
    await test("'\u{0BFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0CFFFE}' === '\\u{0CFFFE}'");
    await test("'\u{0CFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0CFFFF}' === '\\u{0CFFFF}'");
    await test("'\u{0CFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0DFFFE}' === '\\u{0DFFFE}'");
    await test("'\u{0DFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0DFFFF}' === '\\u{0DFFFF}'");
    await test("'\u{0DFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0EFFFE}' === '\\u{0EFFFE}'");
    await test("'\u{0EFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0EFFFF}' === '\\u{0EFFFF}'");
    await test("'\u{0EFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{0FFFFE}' === '\\u{0FFFFE}'");
    await test("'\u{0FFFFE}'.codePointAt(0).toString(16)");
    await test("'\u{0FFFFF}' === '\\u{0FFFFF}'");
    await test("'\u{0FFFFF}'.codePointAt(0).toString(16)");

    await test("'\u{10FFFE}' === '\\u{10FFFE}'");
    await test("'\u{10FFFE}'.codePointAt(0).toString(16)");
    await test("'\u{10FFFF}' === '\\u{10FFFF}'");
    await test("'\u{10FFFF}'.codePointAt(0).toString(16)");

    await test("String.fromCodePoint(0x10FFFF)");

    // Constructs a string with the Unicode code point 0xffff in V8.
    // On the way back to the browser, it will eventually get
    // converted to JSON, e.g. by escaping the character into
    // \uffff for transport. Eventually, as reflected in the
    // test expectation file, it's converted into a 3 byte UTF8
    // sequence for that codepoint, that is 0xef 0xbf 0xbf.
    await test("String.fromCodePoint(0xFFFF)");
  }

  TestRunner.completeTest();
})();

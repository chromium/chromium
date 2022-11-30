// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that invalid css still parses into properties.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected {
          color: blue;
          a property with spaces: red;
          --a-property-with-no-value:;
          content: "forgot the semicolon"
          --next-property: "next property"
      }
      #inspected {
        /* comment */ before property: hey;
        after property: hey; /* comment */
        /*
          multi
          line
          comment
        */
       background color: orange; /* start comment
       end comment */color: black;
       /* double *//*comment*/
       /* staggered
       */ /* comments
       */ /*/ still in comment /* /* */
       color /* internal */ mistake;
       multiple:properties;in:a:single;;::line;
       :;

        foo: bar; /*
        */ bar: foo;
      }
      </style>
      <div id="inspected" style="123">Text</div>
    `);

  await new Promise(x => ElementsTestRunner.selectNodeAndWaitForStyles('inspected', x));

  await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
  TestRunner.completeTest();
})();

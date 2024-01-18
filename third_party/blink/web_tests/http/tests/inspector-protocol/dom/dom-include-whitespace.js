(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let result;

  // The Test page contains a DIV with 5 childNodes: whitespace (linebreak), SPAN, whitespace, SPAN, whitespace
  const {page, session, dp} = await testRunner.startHTML(`<div id=outer>
  <span id=first>First</span>
  <span id=second>Second</span>
  </div>`, 'Tests that DOM.enable(includeWhitespace) works');

  function dumpOuterChildCount(prefix, root) {
    const div = root.children[0].children[1].children[0];
    testRunner.log(prefix + " - childNodeCount: " + div.childNodeCount + ", children: " + div.children.length);
  }

  // With includeWhitespace:all
  await dp.DOM.enable({includeWhitespace:"all"});
  result = (await dp.DOM.getDocument({depth:-1})).result;

  dumpOuterChildCount("includeWhitespace:all", result.root);

  // With includeWhitespace:none
  await dp.DOM.disable();
  await dp.DOM.enable({includeWhitespace:"none"});
  result = (await dp.DOM.getDocument({depth:-1})).result;

  dumpOuterChildCount("includeWhitespace:none", result.root);

  // With ignoreWhitespaces default ("none")
  await dp.DOM.disable();
  await dp.DOM.enable();
  result = (await dp.DOM.getDocument({depth:-1})).result;

  dumpOuterChildCount("includeWhitespace default", result.root);

  // With implicit enable from getDocument (= default)
  await dp.DOM.disable();
  result = (await dp.DOM.getDocument({depth:-1})).result;

  dumpOuterChildCount("implicit", result.root)

  testRunner.completeTest();
})

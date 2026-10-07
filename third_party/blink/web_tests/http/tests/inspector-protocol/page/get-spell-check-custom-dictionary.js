(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<iframe srcdoc="<p>child</p>"></iframe>`,
      `Tests that Page.getSpellCheckCustomDictionary returns each frame's spell check custom dictionary words.\n`);

  const {result: {frameTree}} = await dp.Page.getFrameTree();
  const mainFrameId = frameTree.frame.id;
  const childFrameId = frameTree.childFrames[0].frame.id;

  async function logWords(label, frameId) {
    const {result} = await dp.Page.getSpellCheckCustomDictionary({frameId});
    testRunner.log(`${label}: ${JSON.stringify(result.words)}`);
  }

  await session.evaluate(`
    testRunner.setMockSpellCheckerEnabled(true);
    frames[0].testRunner.setMockSpellCheckerEnabled(true);
  `);

  testRunner.log('Before any words are added:');
  await logWords('main frame', mainFrameId);
  await logWords('child frame', childFrameId);

  // Invalid and duplicate words are not stored.
  await session.evaluate(`
    document.spellCheckCustomDictionary.addWords(
        ['zeta', 'alpha', 'Pikachu', 'alpha', '', ' padded']);
    frames[0].document.spellCheckCustomDictionary.addWords(['kappa']);
  `);
  testRunner.log('After adding words:');
  await logWords('main frame', mainFrameId);
  await logWords('child frame', childFrameId);

  await session.evaluate(
      `document.spellCheckCustomDictionary.removeWords(['zeta'])`);
  testRunner.log('After removing a word from the main frame:');
  await logWords('main frame', mainFrameId);
  await logWords('child frame', childFrameId);

  const response =
      await dp.Page.getSpellCheckCustomDictionary({frameId: 'no-such-frame'});
  testRunner.log(`Unknown frame: ${response.error.message}`);

  testRunner.completeTest();
})

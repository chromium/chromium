(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startHTML(
      `
    <div id="target">Test mutation</div>
  `,
      'Tests that mutating a constructed stylesheet fires CSS.styleSheetChanged event');

  // Create two empty constructed stylesheets and adopt them.
  await session.evaluate(() => {
    window.sheet1 = new CSSStyleSheet();
    window.sheet2 = new CSSStyleSheet();
    document.adoptedStyleSheets = [window.sheet1, window.sheet2];
  });

  dp.DOM.enable();
  dp.CSS.enable();

  // Wait for the two stylesheets to be added.
  testRunner.log('Waiting for initial stylesheets to be added...');
  await dp.CSS.onceStyleSheetAdded();
  await dp.CSS.onceStyleSheetAdded();
  testRunner.log('Both sheets added.');

  // 1. Mutate sheet1 to a new text (cache miss, populates cache).
  {
    const promise = dp.CSS.onceStyleSheetChanged();
    await session.evaluate(() => {
      window.sheet1.replaceSync('div { color: red; }');
    });
    testRunner.log(
        'Waiting for styleSheetChanged event after sheet1.replaceSync (cache miss)...');
    await promise;
    testRunner.log('Received styleSheetChanged event!');
  }

  // 2. Mutate sheet2 to the SAME text (cache hit, shares cached
  // representation!).
  {
    const promise = dp.CSS.onceStyleSheetChanged();
    await session.evaluate(() => {
      window.sheet2.replaceSync('div { color: red; }');
    });
    testRunner.log(
        'Waiting for styleSheetChanged event after sheet2.replaceSync (cache hit)...');
    await promise;
    testRunner.log('Received styleSheetChanged event!');
  }

  // 3. Mutate sheet2 via replace (async) to a different text (breaks sharing,
  // cache miss).
  {
    const promise = dp.CSS.onceStyleSheetChanged();
    await session.evaluate(async () => {
      await window.sheet2.replace('div { color: blue; }');
    });
    testRunner.log(
        'Waiting for styleSheetChanged event after sheet2.replace (breaks sharing)...');
    await promise;
    testRunner.log('Received styleSheetChanged event!');
  }

  // 4. Test <style> elements mutation via textContent (two style elements)
  {
    testRunner.log('Creating two style elements...');
    const addedPromise1 = dp.CSS.onceStyleSheetAdded();
    const addedPromise2 = dp.CSS.onceStyleSheetAdded();
    await session.evaluate(() => {
      window.styleEl1 = document.createElement('style');
      window.styleEl2 = document.createElement('style');
      document.head.appendChild(window.styleEl1);
      document.head.appendChild(window.styleEl2);
    });
    await addedPromise1;
    await addedPromise2;
    testRunner.log('Both style elements added.');

    // Mutate styleEl1 (cache miss, populates cache).
    {
      const removedPromise = dp.CSS.onceStyleSheetRemoved();
      const addedPromise = dp.CSS.onceStyleSheetAdded();
      await session.evaluate(() => {
        window.styleEl1.textContent = 'div { color: red; }';
      });
      testRunner.log('Waiting for styleEl1 textContent update...');
      await removedPromise;
      await addedPromise;
      testRunner.log('styleEl1 updated.');
    }

    // Mutate styleEl2 to same text (cache hit, shares cached representation!).
    {
      const removedPromise = dp.CSS.onceStyleSheetRemoved();
      const addedPromise = dp.CSS.onceStyleSheetAdded();
      await session.evaluate(() => {
        window.styleEl2.textContent = 'div { color: red; }';
      });
      testRunner.log('Waiting for styleEl2 textContent update (cache hit)...');
      await removedPromise;
      await addedPromise;
      testRunner.log('styleEl2 updated.');
    }

    // Mutate styleEl2 to different text (breaks sharing).
    {
      const removedPromise = dp.CSS.onceStyleSheetRemoved();
      const addedPromise = dp.CSS.onceStyleSheetAdded();
      await session.evaluate(() => {
        window.styleEl2.textContent = 'div { color: blue; }';
      });
      testRunner.log(
          'Waiting for styleEl2 textContent update (breaks sharing)...');
      await removedPromise;
      await addedPromise;
      testRunner.log('styleEl2 updated.');
    }
  }

  testRunner.completeTest();
});

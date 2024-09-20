(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    let {page, session, dp} = await testRunner.startHTML(`
  <script type="module">
    const styleSheetFromConstructor = new CSSStyleSheet();
    styleSheetFromConstructor.replaceSync("div { background-color: 'blue' }");

    import styleSheetFromModule from '../resources/css-module.php?url=css-module.css' with { type: 'css' };

    document.adoptedStyleSheets = [styleSheetFromConstructor, styleSheetFromModule];
  </script>`, 'Check sourceURL of constructed stylesheets, from `new` and from CSS module import');

    dp.DOM.enable();
    dp.CSS.enable();

    const styleSheets = [];
    for (let i = 0; i < 2; ++i)
      styleSheets.push(await dp.CSS.onceStyleSheetAdded());

    styleSheets.sort((a,b) => a.params.header.sourceURL.localeCompare(b.params.header.sourceURL));

    for (const styleSheet of styleSheets) {
      testRunner.log(`isConstructed: ${styleSheet.params.header.isConstructed}`);
      const sourceURL = styleSheet.params.header.sourceURL;
      const trimmedSourceURL = sourceURL.substring(sourceURL.lastIndexOf("/") + 1);
      testRunner.log(`sourceURL: '${trimmedSourceURL}'`);
    }

    testRunner.completeTest();
  });

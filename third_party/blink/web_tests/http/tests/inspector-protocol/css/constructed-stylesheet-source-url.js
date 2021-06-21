(async function(testRunner) {
    let {page, session, dp} = await testRunner.startHTML(`
  <script type="module">
    const styleSheetFromConstructor = new CSSStyleSheet();
    styleSheetFromConstructor.replaceSync("div { background-color: 'blue' }");

    import styleSheetFromModule from '../resources/css-module.php?url=css-module.css' assert { type: 'css' };

    document.adoptedStyleSheets = [styleSheetFromConstructor, styleSheetFromModule];
  </script>`, 'Check sourceURL of constructed stylesheets, from `new` and from CSS module import');

    dp.DOM.enable();
    dp.CSS.enable();

    var styleSheets = [];
    for (var i = 0; i < 2; ++i)
      styleSheets.push(await dp.CSS.onceStyleSheetAdded());

    for (var styleSheet of styleSheets) {
      testRunner.log(`isConstructed: ${styleSheet.params.header.isConstructed}`);
      const sourceURL = styleSheet.params.header.sourceURL;
      const trimmedSourceURL = sourceURL.substring(sourceURL.lastIndexOf("/") + 1);
      testRunner.log(`sourceURL: '${trimmedSourceURL}'`);
    }

    testRunner.completeTest();
  });

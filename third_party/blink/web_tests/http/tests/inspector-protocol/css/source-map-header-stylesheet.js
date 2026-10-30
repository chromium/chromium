(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that SourceMap and X-SourceMap HTTP headers take precedence over sourceMappingURL comments in stylesheets.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function addStylesheet(query) {
    const url =
        testRunner.url(`../resources/source-map-stylesheet.php?${query}`);
    session.evaluate(`
      (() => {
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.href = ${JSON.stringify(url)};
        document.head.appendChild(link);
      })()
    `);
    const {params: {header}} = await dp.CSS.onceStyleSheetAdded();
    testRunner.log(`Query "${query}": sourceMapURL='${header.sourceMapURL}'`);
    return header;
  }

  const headerSheet = await addStylesheet(
      'header=/from-header.css.map&comment=/from-comment.css.map');
  await addStylesheet(
      'xheader=/from-x-header.css.map&comment=/from-comment.css.map');
  await addStylesheet(
      'header=/from-header.css.map&xheader=/from-x-header.css.map&comment=/from-comment.css.map');
  await addStylesheet('comment=/from-comment.css.map');

  const {result: {sourceMapURL}} = await dp.CSS.setStyleSheetText({
    styleSheetId: headerSheet.styleSheetId,
    text:
        'body { color: blue; }\n/*# sourceMappingURL=/from-edited-comment.css.map */',
  });
  testRunner.log(
      `After CSS.setStyleSheetText with SourceMap header: sourceMapURL='${
          sourceMapURL}'`);

  testRunner.completeTest();
});

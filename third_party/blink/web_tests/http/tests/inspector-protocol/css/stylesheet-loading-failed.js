(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {dp} = await testRunner.startHTML(
      `
      <link rel="stylesheet" href="404.css">
      <link rel="stylesheet" href="../../resources/dummy.css">
      <style>
      div {}
      </style>
    `,
      'Report stylesheet loading failures');

  dp.DOM.enable();
  dp.CSS.enable();

  const styleSheets = [];
  for (let i = 0; i < 3; ++i) {
    styleSheets.push(await dp.CSS.onceStyleSheetAdded());
  }

  styleSheets.sort(
      (a, b) =>
          a.params.header.sourceURL.localeCompare(b.params.header.sourceURL));

  for (const styleSheet of styleSheets) {
    const {sourceURL, loadingFailed} = styleSheet.params.header;
    testRunner.log(
        `sourceURL: '${sourceURL.substring(sourceURL.lastIndexOf('/') + 1)}'`);
    testRunner.log(`loadingFailed: ${loadingFailed}`);
  }

  testRunner.completeTest();
});

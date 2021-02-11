(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div style='height: 10000px;'>spacer</div>
    <div id='activatable' style='content-visibility: auto'>
      locked
      <div id='child'>
        child
        <div id='grandChild'>grandChild</div>
      </div>
      <div id='invisible' style='display:none'>invisible</div>
      <div id='nested' style='content-visibility: auto'>nested</div>
      text
    </div>
    <div id='nonViewportActivatable' style='content-visibility: hidden-matchable'>nonViewportActivatable text</div>
    <div id='nonActivatable' style='content-visibility: hidden'>nonActivatable text</div>
    <div id='normal'>normal text</div>
  `, 'Tests accessibility values of display locked nodes');
  const dumpAccessibilityNodesFromList =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodesFromList.js'))(testRunner, session);

  const {result} = await dp.Accessibility.getFullAXTree();
  dumpAccessibilityNodesFromList(result.nodes);
});

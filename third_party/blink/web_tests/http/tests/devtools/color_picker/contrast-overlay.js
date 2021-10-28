// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the contrast line algorithm produces good results and terminates.\n`);


  await TestRunner.loadLegacyModule('color_picker');

  var colorPairs = [
    // Boring black on white
    {fg: 'black', bg: 'white'},
    // Blue on white - line does not go to RHS
    {fg: 'blue', bg: 'white'},
    // Transparent on white - no possible line
    {fg: 'transparent', bg: 'white'},
    // White on color which previously caused infinite loop
    {fg: 'rgba(255, 255, 255, 1)', bg: 'rgba(157, 83, 95, 1)'}
  ];

  function logLineForColorPair(fgColorString, bgColorString, level) {
    var contrastInfoData = {
      backgroundColors: [bgColorString],
      computedFontSize: '16px',
      computedFontWeight: '400',
      computedBodyFontSize: '16px'
    };
    var contrastInfo = new ColorPicker.ContrastInfo(contrastInfoData);
    contrastInfo.setColor(Common.Color.parse(fgColorString));
    var contrastLineBuilder = new ColorPicker.ContrastRatioLineBuilder(contrastInfo);
    var d = contrastLineBuilder.drawContrastRatioLine(100, 100, level);

    TestRunner.addResult('');
    TestRunner.addResult(
        'For fgColor ' + fgColorString + ', bgColor ' + bgColorString + ', ' + level + ' path was' + (d ? '' : ' empty.'));
    if (d)
      TestRunner.addResult(d);
  }

  for (let level of ['aa', 'aaa']) {
    for (var pair of colorPairs)
      logLineForColorPair(pair.fg, pair.bg, level);
  }

  TestRunner.completeTest();
})();

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that the displayed string for colors correctly handles clipped CSS values and RGB percentages.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  var colors = [
    // Each of these is red. Some may need to be clipped to [0, 255].
    'red', '#F00', '#F00F', '#FF0000', '#FF0000FF', 'rgb(255,0,0)',
    'rgb(300,0,0)',       // clipped to rgb(255,0,0)
    'rgb(255,-10,0)',     // clipped to rgb(255,0,0)
    'rgb(110%, 0%, 0%)',  // clipped to rgb(100%,0%,0%)

    // Each of these are valid
    'rgba(0,0,0,0.5)', 'rgb(0,0,0,50%)', 'rgb( 0  0   0   /   50%  )', 'rgb(1 1 1/1)', 'rgb(1 1 1/ 1)',
    'rgba(1.5 1.5 1.5)', 'hsl(-120, 100%, 50%)', 'hsl(-120deg, 100%, 50%)',
    'hsl(-120, 200%, 200%)',         // clipped to hsl(240,100%,100%)
    'hsl(-120, -200%, -200%)',       // clipped to hsl(240,100%,100%)
    'hsla(-120, -200%, -200%, -5)',  // clipped to hsla(0,0%,0%,0)
    'hsla(240,100%,50%,0.05)', 'hsl(200.5,0%,50%)', 'hsla(200,1.5%,50%,1)', 'rgba(0,0,0,.5)', 'hsla(.5,.5%,.5%,.5)',
    'hsla(100.5,50.5%,50.5%,.5)',
    'hwb(-120 200% 200%)',         // clipped to hwb(240 100% 100%) = hwb(0 50% 50%)
    'hwb(-120 -200% -200%)',       // clipped to hwb(240 100% 100%)
    'hwb(-120 -200% -200% / -5)',  // clipped to hwb(0 0% 0% / 0%)
    'hwb(240 100% 50% / 0.05)', 'hwb(200.5 0% 50%)', 'hwb(200 1.5% 50% / 1)', 'hwb(0 0 0 /.5)', 'hwb(.5 .5% .5% .5)',
    'hwb(100grad 20% 30%)', 'hwb(1rad 5% 15%)', 'hwb(1turn 25% 15%)',
    // Each of these has their alpha clipped [0.0, 1.0].
    'rgba(255, 0, 0, -5)',  // clipped to rgba(255,0,0,0)
    'rgba(255, 0, 0, 5)',   // clipped to rgba(255,0,0,1)
  ];

  var invalidColors = [
    // An invalid color, eg a value for a shorthand like 'border' which can have a color
    'none', '#00000', '#ggg', 'rgb(a,b,c)', 'rgb(a,b,c,d)', 'rgba(0 0 0 1%)', 'rgba(0,0,0,)',
    'rgba(0 0, 0)', 'rgba(1 1 1 / )', 'rgb(1 1 / 1)', 'rgb(1 1/1)', 'hsl(0,0,0)', 'hsl(0%, 0%, 0%)',
    'hsla(0,,0,1)', 'hsl(0, 0%, 0)', 'hsl(a,b,c)', 'hsla(0,0,0,0)', 'hsla(0 0% 0% 0)', 'hsla(0 turn, 0, 0, 0)', 'hsla',
    'hwb(0,0,1)', 'hwb(0 0% 0)', 'hwb(a b c)', 'hwb(0 0 0 / 0)', 'hwb(0 0% 0% 0)', 'hwb(0 turn 0 0 0)', 'hwb'
  ];

  TestRunner.runTestSuite([
    function testColors(next) {
      for (var i = 0; i < colors.length; ++i)
        dumpColorRepresentationsForColor(colors[i]);
      next();
    },
    function testInvalidColors(next) {
      for (var i = 0; i < invalidColors.length; ++i)
        dumpErrorsForInvalidColor(invalidColors[i]);
      next();
    },
  ]);

  function dumpErrorsForInvalidColor(colorString) {
    var color = Common.Color.parse(colorString);
    if (!color) {
      TestRunner.addResult('');
      TestRunner.addResult('SUCCESS: parsed invalid color ' + colorString + ' to null');
    } else {
      TestRunner.addResult('');
      TestRunner.addResult('FAIL: invalid color ' + colorString + ' did not parse to to null');
    }
  }

  function dumpColorRepresentationsForColor(colorString) {
    var color = Common.Color.parse(colorString);
    if (!color)
      return;

    TestRunner.addResult('');
    TestRunner.addResult('color: ' + colorString);
    TestRunner.addResult('  simple: ' + !color.hasAlpha());
    var cf = Common.Color.Format;
    for (var colorFormatKey in cf) {
      var colorFormat = cf[colorFormatKey];
      // Simple colors do not have RGBA and HSLA representations.
      if (!color.hasAlpha() && (colorFormat === cf.RGBA || colorFormat === cf.HSLA))
        continue;
      // Skip alpha representation for HWB - only exists internally
      if (colorFormat === cf.HWBA)
        continue;
      // Advanced colors do not have HEX representations.
      if (color.hasAlpha() && (colorFormat === cf.ShortHEX || colorFormat === cf.HEX))
        continue;
      // If there is no ShortHEX then skip it.
      if (colorFormat === cf.ShortHEX && color.detectHEXFormat() !== cf.ShortHEX)
        continue;
      // If there is no nickname, then skip it.
      if (colorFormat === cf.Nickname && !color.nickname())
        continue;
      TestRunner.addResult('  ' + colorFormat + ': ' + color.asString(colorFormat));
    }
  }
})();

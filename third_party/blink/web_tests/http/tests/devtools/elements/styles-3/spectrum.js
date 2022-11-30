// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests ColorPicker.Spectrum\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  function setColor(inputColor, format) {
    TestRunner.addResult('Testing: ' + inputColor);
    var color = Common.Color.parse(inputColor);
    spectrum.setColor(color, format);
  }

  function checkColorString(inputColor, format) {
    setColor(inputColor, format);
    TestRunner.addResult(spectrum.colorString());
  }

  function checkAlphaChange(inputColor, format) {
    setColor(inputColor, format);
    spectrum.hsv[3] = 0;
    spectrum.innerSetColor(spectrum._hsv, undefined, undefined /* colorName */, undefined, ColorPicker.Spectrum._ChangeSource.Other);
    TestRunner.addResult(spectrum.colorString());
  }

  function checkNextFormat(inputColor, format) {
    setColor(inputColor, format);
    spectrum.formatViewSwitch();
    TestRunner.addResult(spectrum.colorFormat);
    spectrum.formatViewSwitch();
    TestRunner.addResult(spectrum.colorFormat);
  }

  var spectrum = new ColorPicker.Spectrum();
  var cf = Common.Color.Format;
  var inputColors = [
    {string: 'red', format: cf.Nickname}, {string: '#ABC', format: cf.ShortHEX},
    {string: '#ABCA', format: cf.ShortHEXA}, {string: '#ABCDEF', format: cf.HEX},
    {string: '#ABCDEFAA', format: cf.HEXA}, {string: 'rgb(1, 2, 3)', format: cf.RGB},
    {string: 'rgba(1, 2, 3, 0.2)', format: cf.RGB}, {string: 'rgb(1, 2, 3, 0.2)', format: cf.RGB},
    {string: 'rgb(1 2 3 / 20%)', format: cf.RGB}, {string: 'rgbA(1 2 3)', format: cf.RGB},
    {string: 'rgba(1.5 2.6 3.1)', format: cf.RGB}, {string: 'hsl(1, 100%, 50%)', format: cf.HSL},
    {string: 'hsl(1 100% 50%)', format: cf.HSL}, {string: 'hsla(1, 100%, 50%, 0.2)', format: cf.HSLA},
    {string: 'hsl(1 100% 50% / 20%)', format: cf.HSL}, {string: 'hsL(1deg  100% 50%  /  20%)', format: cf.HSL},
    {string: 'hwb(1 100% 50%)', format: cf.HWB}, {string: 'hwb(100grad 100% 50% / 0.2)', format: cf.HWB},
    {string: 'hwb(1rad 20% 50% / 20%)', format: cf.HWB}, {string: 'hwB(1deg  20% 50%  /  20%)', format: cf.HWB}
  ];

  TestRunner.addResult('--- Testing colorString()');
  for (var color of inputColors)
    checkColorString(color.string, color.format);

  TestRunner.addResult('--- Testing alpha changes');
  for (var color of inputColors)
    checkAlphaChange(color.string, color.format);

  TestRunner.addResult('--- Testing _formatViewSwitch()');
  for (var color of inputColors)
    checkNextFormat(color.string, color.format);

  TestRunner.completeTest();
})();

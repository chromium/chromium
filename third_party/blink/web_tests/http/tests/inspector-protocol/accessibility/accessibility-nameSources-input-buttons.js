(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <style>
    body.done .tests {
      display: none;
    }
    </style>
    <script>
    function done() {
      document.body.classList.add('done');
    }
    </script>
    <!-- Compare with accessibility/name-calc-figure.html, accessibility/name-calc-img.html, accessibility/name-calc-presentational.html and accessibility/name-calc-svg.html-->
    <div class='tests'>
      <input id='button1' type='button'>

      <input id='button2' type='button' value='button-value2'>

      <input id='button3' type='button' value='button-value3' title='button-title3'>

      <input id='button4' type='button' title='button-title4'>

      <input id='button5' type='button'>
      <label for='button5'>button-label-5</label>

      <label>button-label-6<input id='button6' type='button'></label>

      <input id='button7' type='button' value='button-value7'>
      <label for='button7'>button-label-7</label>

      <input id='button8' type='button' value='button-value8' aria-label='button-aria-label-8'>
      <label for='button8'>button-label-8</label>

      <input id='button9' type='button' value='button-value9' aria-label='button-aria-label-9' aria-labelledby='label-for-button9'>
      <label for='button9'>button-label-9</label>
      <span id='label-for-button9'>button9-aria-labelledby</span>

      <input id='submit1' type='submit'>

      <input id='submit2' type='submit' value='submit-value2'>

      <input id='submit3' type='submit' title='submit-title'>

      <input id='reset1' type='reset'>

      <input id='image-input1' type='image' src='resources/cake.png'>

      <input id='image-input2' type='image' src='resources/cake.png' value='image-input-value2'>

      <input id='image-input3' type='image' src='resources/cake.png' alt='image-input-alt3'>

      <input id='image-input4' type='image' src='resources/cake.png' alt='image-input-alt4' value='image-input-value4'>

      <input id='image-input5' type='image' src='resources/cake.png' title='image-input-title5'>
    </div>
  `, 'Tests name sources in input[type=button].');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('input', false, msg);
})

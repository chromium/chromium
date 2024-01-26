(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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
      <input data-dump id='text1' type='text'>

      <input data-dump id='text2' type='text' title='text2-title'>

      <input data-dump id='text3' type='text' title='text3-title' aria-placeholder='text3-aria-placeholder' placeholder='text3-placeholder'>

      <input data-dump id='text4' type='text' title='text4-title' aria-placeholder='text4-aria-placeholder' placeholder='text4-placeholder'>
      <label for='text4'>label-for-text4</label>

      <input data-dump id='text5' type='text' title='text5-title' aria-placeholder='text5-aria-placeholder' placeholder='text5-placeholder' aria-label='text5-aria-label'>
      <label for='text5'>label-for-text5</label>

      <input data-dump id='text6' type='text' title='text6-title' aria-placeholder='text6-aria-placeholder' placeholder='text6-placeholder' aria-label='text6-aria-label' aria-labelledby='text-labelledby6'>
      <label for='text6'>label-for-text6</label>
      <span id='text-labelledby6'>labelledby-for-text6</span>

      <label>label-wrapping-text7<input data-dump id='text7' type='text' title='text7-title'></label>

      <label for='dummy'>label-wrapping-text8<input data-dump id='text8' type='text'></label>

      <label for='text9'>label-for-text9</label>
      <label>label-wrapping-text9<input data-dump id='text9' type='text' title='text9-title' aria-placeholder='text9-aria-placeholder' placeholder='text9-placeholder'></label>

      <label>label-wrapping-text10<input data-dump id='text10' type='text' title='text10-title' aria-placeholder='text10-aria-placeholder' placeholder='text10-placeholder'></label>

      <input data-dump id='text11' type='text'>
      <label for='text11'>first-label-for-text11</label>
      <label for='text11'>second-label-for-text11</label>

      <input data-dump id='text12' type='text' title='text12-title' aria-placeholder='text12-aria-placeholder'>
  </div>
  `, 'Tests name sources in inputs.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('[data-dump]', false, msg);
})

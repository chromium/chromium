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
      <!-- Need to test summary element which has no DOM node equivalent.
           <details id='details1'>
             <p>details1-content</p>
           </details>
           -->

      <details id='details2'>
        <summary id='summary2' title='summary2-title'></summary>
        <p>details2-content</p>
      </details>

      <details id='details3'>
        <summary id='summary3' title='summary3-title'>summary3-contents</summary>
        <p>details3-content</p>
      </details>

      <details id='details4'>
        <summary id='summary4' title='summary4-title' aria-label='summary4-aria-label'>summary4-contents</summary>
        <p>details4-content</p>
      </details>

      <details id='details5'>
        <summary id='summary5' title='summary5-title' aria-label='summary5-aria-label' aria-labelledby='labelledby5'>summary5-contents</summary>
        <p>details5-content</p>
      </details>
      <span hidden='true' id='labelledby5'>summary5-aria-labelledby</span>
    </div>
  `, 'Tests name sources in details and summary.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('summary', false, msg);
})
